#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "daily_robotics_2026_10_22/msg/optimization_stats.hpp"
#include "daily_robotics_2026_10_22/msg/pose_graph_input.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"

namespace
{
constexpr std::size_t kMaxPoses = 12U;
constexpr std::size_t kMaxLoops = 3U;
constexpr std::size_t kPoseDimensions = 3U;
constexpr std::size_t kMaxVariables = (kMaxPoses - 1U) * kPoseDimensions + kMaxLoops;
constexpr std::size_t kIterations = 8U;
constexpr double kFiniteDifference = 1.0e-6;
constexpr double kSwitchPriorWeight = 12.0;

struct Pose2
{
  double x{};
  double y{};
  double yaw{};
};

struct Edge2
{
  std::size_t from{};
  std::size_t to{};
  Pose2 measurement{};
};

struct SolveResult
{
  std::array<Pose2, kMaxPoses> poses{};
  std::array<double, kMaxLoops> switches{{1.0, 1.0, 1.0}};
  double cost{std::numeric_limits<double>::infinity()};
  bool ok{false};
};

using Vector = std::array<double, kMaxVariables>;
using Matrix = std::array<std::array<double, kMaxVariables>, kMaxVariables>;

double wrap_angle(double angle)
{
  return std::atan2(std::sin(angle), std::cos(angle));
}

// e_ij = Log(z_ij^{-1} * (T_i^{-1} T_j))의 교육용 SE(2) 좌표 표현이다.
// 위치 차이를 i 좌표계로 회전하고 측정값을 빼며, yaw 오차는 [-pi,pi]로 감싼다.
std::array<double, 3U> edge_residual(
  const Pose2 & from, const Pose2 & to, const Pose2 & measurement)
{
  const double dx_world = to.x - from.x;
  const double dy_world = to.y - from.y;
  const double c = std::cos(from.yaw);
  const double s = std::sin(from.yaw);
  const double predicted_x = c * dx_world + s * dy_world;
  const double predicted_y = -s * dx_world + c * dy_world;
  return {
    predicted_x - measurement.x,
    predicted_y - measurement.y,
    wrap_angle(wrap_angle(to.yaw - from.yaw) - measurement.yaw)};
}

void perturb(Pose2 & pose, std::size_t component, double amount)
{
  if (component == 0U) {
    pose.x += amount;
  } else if (component == 1U) {
    pose.y += amount;
  } else {
    pose.yaw = wrap_angle(pose.yaw + amount);
  }
}

int pose_variable(std::size_t pose, std::size_t component)
{
  // 첫 pose는 map gauge freedom을 없애기 위해 고정한다. 따라서 최적화 변수에 넣지 않는다.
  if (pose == 0U) {
    return -1;
  }
  return static_cast<int>((pose - 1U) * kPoseDimensions + component);
}

// 한 3차원 factor의 J^T J와 J^T r를 고정 크기 normal equation에 누적한다.
// local_variables는 pose 6개와 선택적인 switch 1개뿐이라 최대 7로 제한된다.
void accumulate_factor(
  Matrix & hessian, Vector & gradient,
  const std::array<double, 3U> & residual,
  const std::array<std::array<double, 7U>, 3U> & jacobian,
  const std::array<int, 7U> & indices, std::size_t local_variables,
  const std::array<double, 3U> & sqrt_information)
{
  for (std::size_t row = 0U; row < 3U; ++row) {
    const double weighted_residual = sqrt_information[row] * residual[row];
    for (std::size_t a = 0U; a < local_variables; ++a) {
      if (indices[a] < 0) {
        continue;
      }
      const auto global_a = static_cast<std::size_t>(indices[a]);
      const double weighted_jacobian_a = sqrt_information[row] * jacobian[row][a];
      gradient[global_a] += weighted_jacobian_a * weighted_residual;
      for (std::size_t b = 0U; b < local_variables; ++b) {
        if (indices[b] < 0) {
          continue;
        }
        const auto global_b = static_cast<std::size_t>(indices[b]);
        const double weighted_jacobian_b = sqrt_information[row] * jacobian[row][b];
        hessian[global_a][global_b] += weighted_jacobian_a * weighted_jacobian_b;
      }
    }
  }
}

// 부분 피벗 Gaussian elimination으로 H*delta=-g를 푼다.
// std::vector나 동적 할당 없이 최대 36x36 고정 배열만 사용한다.
bool solve_linear_system(Matrix matrix, Vector rhs, std::size_t dimension, Vector & solution)
{
  solution.fill(0.0);
  for (std::size_t column = 0U; column < dimension; ++column) {
    std::size_t pivot = column;
    double pivot_size = std::abs(matrix[column][column]);
    for (std::size_t row = column + 1U; row < dimension; ++row) {
      const double candidate = std::abs(matrix[row][column]);
      if (candidate > pivot_size) {
        pivot = row;
        pivot_size = candidate;
      }
    }
    if (!std::isfinite(pivot_size) || pivot_size < 1.0e-10) {
      return false;
    }
    if (pivot != column) {
      std::swap(matrix[pivot], matrix[column]);
      std::swap(rhs[pivot], rhs[column]);
    }
    for (std::size_t row = column + 1U; row < dimension; ++row) {
      const double ratio = matrix[row][column] / matrix[column][column];
      matrix[row][column] = 0.0;
      for (std::size_t inner = column + 1U; inner < dimension; ++inner) {
        matrix[row][inner] -= ratio * matrix[column][inner];
      }
      rhs[row] -= ratio * rhs[column];
    }
  }
  for (std::size_t reverse = dimension; reverse > 0U; --reverse) {
    const std::size_t row = reverse - 1U;
    double value = rhs[row];
    for (std::size_t column = row + 1U; column < dimension; ++column) {
      value -= matrix[row][column] * solution[column];
    }
    solution[row] = value / matrix[row][row];
    if (!std::isfinite(solution[row])) {
      return false;
    }
  }
  return true;
}

std::array<std::array<double, 6U>, 3U> numerical_pose_jacobian(
  const Pose2 & from, const Pose2 & to, const Pose2 & measurement)
{
  std::array<std::array<double, 6U>, 3U> jacobian{};
  const auto base = edge_residual(from, to, measurement);
  for (std::size_t variable = 0U; variable < 6U; ++variable) {
    Pose2 perturbed_from = from;
    Pose2 perturbed_to = to;
    if (variable < 3U) {
      perturb(perturbed_from, variable, kFiniteDifference);
    } else {
      perturb(perturbed_to, variable - 3U, kFiniteDifference);
    }
    const auto changed = edge_residual(perturbed_from, perturbed_to, measurement);
    for (std::size_t row = 0U; row < 3U; ++row) {
      const double difference = (row == 2U) ?
        wrap_angle(changed[row] - base[row]) : changed[row] - base[row];
      jacobian[row][variable] = difference / kFiniteDifference;
    }
  }
  return jacobian;
}

void add_edge(
  Matrix & hessian, Vector & gradient, const std::array<Pose2, kMaxPoses> & poses,
  const Edge2 & edge, const std::array<double, 3U> & sqrt_information,
  double switch_value, int switch_variable)
{
  const auto base_residual = edge_residual(poses[edge.from], poses[edge.to], edge.measurement);
  const auto pose_jacobian = numerical_pose_jacobian(
    poses[edge.from], poses[edge.to], edge.measurement);
  std::array<double, 3U> residual{};
  std::array<std::array<double, 7U>, 3U> jacobian{};
  std::array<int, 7U> indices{};
  indices.fill(-1);

  for (std::size_t component = 0U; component < 3U; ++component) {
    indices[component] = pose_variable(edge.from, component);
    indices[component + 3U] = pose_variable(edge.to, component);
    residual[component] = switch_value * base_residual[component];
    for (std::size_t variable = 0U; variable < 6U; ++variable) {
      jacobian[component][variable] = switch_value * pose_jacobian[component][variable];
    }
    // d(s*e)/ds=e: switch가 residual을 켜고 끄는 정도를 최적화가 직접 배운다.
    jacobian[component][6U] = base_residual[component];
  }
  indices[6U] = switch_variable;
  accumulate_factor(
    hessian, gradient, residual, jacobian, indices,
    switch_variable >= 0 ? 7U : 6U, sqrt_information);
}

double squared_cost(
  const std::array<Pose2, kMaxPoses> & poses,
  const std::array<Edge2, kMaxPoses - 1U> & odometry, std::size_t odometry_count,
  const std::array<Edge2, kMaxLoops> & loops, std::size_t loop_count,
  const std::array<double, kMaxLoops> & switches, bool switchable)
{
  const std::array<double, 3U> odometry_information{{100.0, 100.0, 180.0}};
  const std::array<double, 3U> loop_information{{400.0, 400.0, 250.0}};
  double cost = 0.0;
  for (std::size_t edge = 0U; edge < odometry_count; ++edge) {
    const auto residual = edge_residual(
      poses[odometry[edge].from], poses[odometry[edge].to], odometry[edge].measurement);
    for (std::size_t row = 0U; row < 3U; ++row) {
      cost += odometry_information[row] * residual[row] * residual[row];
    }
  }
  for (std::size_t edge = 0U; edge < loop_count; ++edge) {
    const auto residual = edge_residual(
      poses[loops[edge].from], poses[loops[edge].to], loops[edge].measurement);
    const double scale = switchable ? switches[edge] : 1.0;
    for (std::size_t row = 0U; row < 3U; ++row) {
      cost += loop_information[row] * scale * scale * residual[row] * residual[row];
    }
    if (switchable) {
      const double prior_residual = 1.0 - scale;
      cost += kSwitchPriorWeight * prior_residual * prior_residual;
    }
  }
  return cost;
}

SolveResult optimize(
  const daily_robotics_2026_10_22::msg::PoseGraphInput & input, bool switchable)
{
  SolveResult result;
  const std::size_t pose_count = static_cast<std::size_t>(input.pose_count);
  const std::size_t odometry_count = static_cast<std::size_t>(input.odometry_count);
  const std::size_t loop_count = static_cast<std::size_t>(input.loop_count);
  for (std::size_t pose = 0U; pose < pose_count; ++pose) {
    result.poses[pose] = Pose2{
      input.initial_x[pose], input.initial_y[pose], input.initial_yaw[pose]};
  }

  std::array<Edge2, kMaxPoses - 1U> odometry{};
  for (std::size_t edge = 0U; edge < odometry_count; ++edge) {
    odometry[edge] = Edge2{
      input.odometry_from[edge], input.odometry_to[edge],
      {input.odometry_dx[edge], input.odometry_dy[edge], input.odometry_dyaw[edge]}};
  }
  std::array<Edge2, kMaxLoops> loops{};
  for (std::size_t edge = 0U; edge < loop_count; ++edge) {
    loops[edge] = Edge2{
      input.loop_from[edge], input.loop_to[edge],
      {input.loop_dx[edge], input.loop_dy[edge], input.loop_dyaw[edge]}};
  }

  const std::size_t pose_variables = (pose_count - 1U) * kPoseDimensions;
  const std::size_t dimension = pose_variables + (switchable ? loop_count : 0U);
  const std::array<double, 3U> odometry_sqrt_information{{10.0, 10.0, std::sqrt(180.0)}};
  const std::array<double, 3U> loop_sqrt_information{{20.0, 20.0, std::sqrt(250.0)}};

  bool all_iterations_ok = true;
  for (std::size_t iteration = 0U; iteration < kIterations; ++iteration) {
    Matrix hessian{};
    Vector gradient{};
    for (std::size_t edge = 0U; edge < odometry_count; ++edge) {
      add_edge(hessian, gradient, result.poses, odometry[edge], odometry_sqrt_information, 1.0, -1);
    }
    for (std::size_t edge = 0U; edge < loop_count; ++edge) {
      const int switch_index = switchable ?
        static_cast<int>(pose_variables + edge) : -1;
      const double switch_value = switchable ? result.switches[edge] : 1.0;
      add_edge(
        hessian, gradient, result.poses, loops[edge], loop_sqrt_information,
        switch_value, switch_index);
      if (switchable) {
        // switch prior ||1-s||^2는 정상 loop를 켜 두려는 약한 선호다.
        // 데이터 residual이 매우 크면 이 prior보다 loop를 끄는 편이 더 싸다.
        const auto index = static_cast<std::size_t>(switch_index);
        const double residual = std::sqrt(kSwitchPriorWeight) * (1.0 - switch_value);
        const double jacobian = -std::sqrt(kSwitchPriorWeight);
        gradient[index] += jacobian * residual;
        hessian[index][index] += jacobian * jacobian;
      }
    }

    // 작은 Levenberg damping은 거의 특이한 방향에서 피벗 실패를 막는다.
    for (std::size_t diagonal = 0U; diagonal < dimension; ++diagonal) {
      hessian[diagonal][diagonal] += 1.0e-4;
    }
    Vector rhs{};
    for (std::size_t index = 0U; index < dimension; ++index) {
      rhs[index] = -gradient[index];
    }
    Vector delta{};
    if (!solve_linear_system(hessian, rhs, dimension, delta)) {
      all_iterations_ok = false;
      break;
    }
    for (std::size_t pose = 1U; pose < pose_count; ++pose) {
      const std::size_t base = (pose - 1U) * kPoseDimensions;
      // 교육용 solver의 한 iteration 이동량을 제한해 큰 outlier의 첫 step을 안정화한다.
      result.poses[pose].x += std::clamp(delta[base], -0.5, 0.5);
      result.poses[pose].y += std::clamp(delta[base + 1U], -0.5, 0.5);
      result.poses[pose].yaw = wrap_angle(
        result.poses[pose].yaw + std::clamp(delta[base + 2U], -0.35, 0.35));
    }
    if (switchable) {
      for (std::size_t edge = 0U; edge < loop_count; ++edge) {
        const double updated = result.switches[edge] +
          std::clamp(delta[pose_variables + edge], -0.5, 0.5);
        // 원 논문의 switch function을 단순화한 실습 경계다. 물리적 의미는 [0,1]이다.
        result.switches[edge] = std::clamp(updated, 0.0, 1.0);
      }
    }
  }
  result.cost = squared_cost(
    result.poses, odometry, odometry_count, loops, loop_count,
    result.switches, switchable);
  result.ok = all_iterations_ok && std::isfinite(result.cost);
  return result;
}

geometry_msgs::msg::PoseStamped to_pose_stamped(
  const Pose2 & pose, const std_msgs::msg::Header & header)
{
  geometry_msgs::msg::PoseStamped output;
  output.header = header;
  output.pose.position.x = pose.x;
  output.pose.position.y = pose.y;
  output.pose.orientation.z = std::sin(0.5 * pose.yaw);
  output.pose.orientation.w = std::cos(0.5 * pose.yaw);
  return output;
}
}  // namespace

class BoundedSwitchableOptimizer : public rclcpp::Node
{
public:
  BoundedSwitchableOptimizer()
  : Node("bounded_switchable_optimizer")
  {
    const auto input_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    const auto output_qos = rclcpp::QoS(rclcpp::KeepLast(5)).reliable();
    robust_path_publisher_ = create_publisher<nav_msgs::msg::Path>(
      "/study/robust_path", output_qos);
    naive_path_publisher_ = create_publisher<nav_msgs::msg::Path>(
      "/study/naive_path", output_qos);
    stats_publisher_ = create_publisher<daily_robotics_2026_10_22::msg::OptimizationStats>(
      "/study/optimization_stats", output_qos);

    // subscription callback 하나가 graph snapshot 전체를 소유해 최적화한다.
    // 동시에 같은 graph state를 수정할 callback이 없으므로 mutex가 필요 없다.
    input_subscription_ = create_subscription<daily_robotics_2026_10_22::msg::PoseGraphInput>(
      "/study/pose_graph_input", input_qos,
      std::bind(&BoundedSwitchableOptimizer::on_graph, this, std::placeholders::_1));
  }

private:
  void on_graph(const daily_robotics_2026_10_22::msg::PoseGraphInput::SharedPtr input)
  {
    if (input->pose_count < 2U || input->pose_count > kMaxPoses ||
      input->odometry_count > kMaxPoses - 1U || input->loop_count > kMaxLoops)
    {
      RCLCPP_ERROR(get_logger(), "invalid bounded graph dimensions; snapshot rejected");
      return;
    }

    const auto begin = std::chrono::steady_clock::now();
    // 같은 초기값에 대해 false loop를 무조건 믿는 기준선과 switchable solver를 모두 실행한다.
    const SolveResult naive = optimize(*input, false);
    const SolveResult robust = optimize(*input, true);
    const auto end = std::chrono::steady_clock::now();
    const double solve_time_us =
      std::chrono::duration<double, std::micro>(end - begin).count();

    nav_msgs::msg::Path naive_path;
    nav_msgs::msg::Path robust_path;
    naive_path.header = input->header;
    robust_path.header = input->header;
    naive_path.poses.reserve(input->pose_count);
    robust_path.poses.reserve(input->pose_count);
    for (std::size_t pose = 0U; pose < input->pose_count; ++pose) {
      naive_path.poses.push_back(to_pose_stamped(naive.poses[pose], input->header));
      robust_path.poses.push_back(to_pose_stamped(robust.poses[pose], input->header));
    }

    daily_robotics_2026_10_22::msg::OptimizationStats stats;
    stats.header = input->header;
    stats.sequence = input->sequence;
    stats.iterations = static_cast<std::uint8_t>(kIterations);
    stats.naive_cost = naive.cost;
    stats.robust_cost = robust.cost;
    stats.loop_switches = robust.switches;
    stats.solve_time_us = solve_time_us;
    stats.solver_ok = naive.ok && robust.ok;

    naive_path_publisher_->publish(naive_path);
    robust_path_publisher_->publish(robust_path);
    stats_publisher_->publish(stats);
    RCLCPP_INFO(
      get_logger(),
      "sequence=%u solved in %.3f us, switch=[%.3f %.3f %.3f], cost naive=%.3f robust=%.3f",
      stats.sequence, stats.solve_time_us, stats.loop_switches[0], stats.loop_switches[1],
      stats.loop_switches[2], stats.naive_cost, stats.robust_cost);
  }

  rclcpp::Subscription<daily_robotics_2026_10_22::msg::PoseGraphInput>::SharedPtr
    input_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr robust_path_publisher_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr naive_path_publisher_;
  rclcpp::Publisher<daily_robotics_2026_10_22::msg::OptimizationStats>::SharedPtr stats_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BoundedSwitchableOptimizer>());
  rclcpp::shutdown();
  return 0;
}
