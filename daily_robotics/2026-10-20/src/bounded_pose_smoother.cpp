#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>

#include "daily_robotics_2026_10_20/msg/fusion_stats.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

namespace
{
constexpr std::size_t kPoseDoF = 6U;
constexpr std::size_t kMaxStates = 8U;
constexpr std::size_t kMaxDimension = kPoseDoF * kMaxStates;
constexpr std::size_t kGaussNewtonIterations = 3U;

std::int64_t stamp_to_nanoseconds(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1000000000LL +
         static_cast<std::int64_t>(stamp.nanosec);
}

std::array<double, 3> rpy_from_quaternion(const geometry_msgs::msg::Quaternion & q_input)
{
  // 비정상 입력이 optimizer에 NaN을 퍼뜨리지 않도록 먼저 단위 쿼터니언으로 정규화한다.
  const double norm = std::sqrt(
    q_input.x * q_input.x + q_input.y * q_input.y +
    q_input.z * q_input.z + q_input.w * q_input.w);
  const double safe_norm = norm > 1.0e-12 ? norm : 1.0;
  const double x = q_input.x / safe_norm;
  const double y = q_input.y / safe_norm;
  const double z = q_input.z / safe_norm;
  const double w = q_input.w / safe_norm;

  const double sin_roll_cos_pitch = 2.0 * (w * x + y * z);
  const double cos_roll_cos_pitch = 1.0 - 2.0 * (x * x + y * y);
  const double roll = std::atan2(sin_roll_cos_pitch, cos_roll_cos_pitch);
  const double sin_pitch = std::clamp(2.0 * (w * y - z * x), -1.0, 1.0);
  const double pitch = std::asin(sin_pitch);
  const double sin_yaw_cos_pitch = 2.0 * (w * z + x * y);
  const double cos_yaw_cos_pitch = 1.0 - 2.0 * (y * y + z * z);
  const double yaw = std::atan2(sin_yaw_cos_pitch, cos_yaw_cos_pitch);
  return {roll, pitch, yaw};
}

geometry_msgs::msg::Quaternion quaternion_from_rpy(const double roll, const double pitch, const double yaw)
{
  const double cr = std::cos(0.5 * roll);
  const double sr = std::sin(0.5 * roll);
  const double cp = std::cos(0.5 * pitch);
  const double sp = std::sin(0.5 * pitch);
  const double cy = std::cos(0.5 * yaw);
  const double sy = std::sin(0.5 * yaw);
  geometry_msgs::msg::Quaternion q;
  q.w = cr * cp * cy + sr * sp * sy;
  q.x = sr * cp * cy - cr * sp * sy;
  q.y = cr * sp * cy + sr * cp * sy;
  q.z = cr * cp * sy - sr * sp * cy;
  return q;
}

std::array<double, 3> previous_body_to_world(
  const std::array<double, 3> & delta_body, const std::array<double, 6> & previous_pose)
{
  // Δp_world = R(previous_pose) Δp_body. 상대 카메라 이동을 map 좌표계 증분으로 바꾼다.
  const double cr = std::cos(previous_pose[3]);
  const double sr = std::sin(previous_pose[3]);
  const double cp = std::cos(previous_pose[4]);
  const double sp = std::sin(previous_pose[4]);
  const double cy = std::cos(previous_pose[5]);
  const double sy = std::sin(previous_pose[5]);
  return {
    cy * cp * delta_body[0] + (cy * sp * sr - sy * cr) * delta_body[1] +
    (cy * sp * cr + sy * sr) * delta_body[2],
    sy * cp * delta_body[0] + (sy * sp * sr + cy * cr) * delta_body[1] +
    (sy * sp * cr - cy * sr) * delta_body[2],
    -sp * delta_body[0] + cp * sr * delta_body[1] + cp * cr * delta_body[2]};
}

double positive_information(const double variance, const double fallback_variance)
{
  const double safe_variance =
    (std::isfinite(variance) && variance > 1.0e-12) ? variance : fallback_variance;
  return 1.0 / safe_variance;
}
}  // namespace

class BoundedPoseSmoother : public rclcpp::Node
{
public:
  BoundedPoseSmoother()
  : Node("bounded_pose_smoother")
  {
    const auto qos = rclcpp::QoS(rclcpp::KeepLast(20)).reliable();

    // 각 콜백은 가변 컨테이너에 쌓지 않고 최대 8개 state/factor 배열만 갱신한다.
    relative_sub_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/sim/relative_pose", qos,
      std::bind(&BoundedPoseSmoother::on_relative_pose, this, std::placeholders::_1));
    absolute_sub_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/sim/absolute_pose", qos,
      std::bind(&BoundedPoseSmoother::on_absolute_pose, this, std::placeholders::_1));

    pose_pub_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>("/fusion/pose", qos);
    stats_pub_ = create_publisher<daily_robotics_2026_10_20::msg::FusionStats>(
      "/fusion/stats", qos);
  }

private:
  struct State
  {
    bool valid{false};
    bool has_absolute{false};
    builtin_interfaces::msg::Time stamp{};
    std::array<double, kPoseDoF> x{};
    // rel[d] = x_i[d] - x_{i-1}[d]인 local tangent-space factor다.
    std::array<double, kPoseDoF> relative{};
    std::array<double, kPoseDoF> relative_information{};
    std::array<double, kPoseDoF> absolute{};
    std::array<double, kPoseDoF> absolute_information{};
  };

  struct PendingAbsolute
  {
    bool valid{false};
    geometry_msgs::msg::PoseWithCovarianceStamped message{};
  };

  void pose_message_to_vector(
    const geometry_msgs::msg::PoseWithCovarianceStamped & message,
    std::array<double, kPoseDoF> & vector) const
  {
    vector[0] = message.pose.pose.position.x;
    vector[1] = message.pose.pose.position.y;
    vector[2] = message.pose.pose.position.z;
    const auto rpy = rpy_from_quaternion(message.pose.pose.orientation);
    vector[3] = rpy[0];
    vector[4] = rpy[1];
    vector[5] = rpy[2];
  }

  void fill_information(
    const geometry_msgs::msg::PoseWithCovarianceStamped & message,
    std::array<double, kPoseDoF> & information, const double fallback_variance) const
  {
    constexpr std::array<std::size_t, kPoseDoF> diagonal_indices{0U, 7U, 14U, 21U, 28U, 35U};
    for (std::size_t d = 0U; d < kPoseDoF; ++d) {
      information[d] = positive_information(
        message.pose.covariance[diagonal_indices[d]], fallback_variance);
    }
  }

  bool add_absolute_to_existing_state(
    const geometry_msgs::msg::PoseWithCovarianceStamped & message)
  {
    const auto measurement_time = stamp_to_nanoseconds(message.header.stamp);
    for (std::size_t i = 0U; i < active_states_; ++i) {
      if (stamp_to_nanoseconds(states_[i].stamp) == measurement_time) {
        states_[i].has_absolute = true;
        pose_message_to_vector(message, states_[i].absolute);
        fill_information(message, states_[i].absolute_information, 0.05 * 0.05);
        if (i + 1U < active_states_) {
          ++delayed_factors_inserted_;
        }
        return true;
      }
    }
    return false;
  }

  void initialize_from_absolute(
    const geometry_msgs::msg::PoseWithCovarianceStamped & message)
  {
    states_[0] = State{};
    states_[0].valid = true;
    states_[0].has_absolute = true;
    states_[0].stamp = message.header.stamp;
    pose_message_to_vector(message, states_[0].x);
    states_[0].absolute = states_[0].x;
    fill_information(message, states_[0].absolute_information, 0.05 * 0.05);
    prior_mean_ = states_[0].x;
    prior_information_.fill(4000.0);
    active_states_ = 1U;
    solve_and_publish();
  }

  void on_absolute_pose(
    const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr message)
  {
    // 절대 자세는 map 프레임이어야 한다. 다른 프레임을 묵시적으로 섞으면 추정기가 그럴듯하게 틀린다.
    if (message->header.frame_id != "map") {
      RCLCPP_WARN(get_logger(), "absolute pose rejected: frame_id must be map");
      return;
    }
    if (active_states_ == 0U) {
      initialize_from_absolute(*message);
      return;
    }
    if (add_absolute_to_existing_state(*message)) {
      solve_and_publish();
      return;
    }

    const auto measurement_time = stamp_to_nanoseconds(message->header.stamp);
    if (measurement_time < stamp_to_nanoseconds(states_[0].stamp)) {
      // fixed lag 밖의 factor는 계산 시간을 무제한으로 늘리지 않기 위해 명시적으로 버린다.
      ++too_old_factors_dropped_;
      return;
    }
    for (auto & pending : pending_absolute_) {
      if (!pending.valid) {
        pending.valid = true;
        pending.message = *message;
        return;
      }
    }
    // 입력 burst가 설계한 8-slot pending 예산을 넘은 경우도 drop으로 계수한다.
    ++too_old_factors_dropped_;
  }

  void shift_window_if_full()
  {
    if (active_states_ < kMaxStates) {
      return;
    }
    for (std::size_t i = 1U; i < kMaxStates; ++i) {
      states_[i - 1U] = states_[i];
    }
    states_[kMaxStates - 1U] = State{};

    // 교육용 bounded marginalization: 새 첫 state의 현재 추정치를 Gaussian prior로 고정한다.
    // 실제 iSAM2/Bayes-tree 또는 Schur complement marginal은 교차상관을 더 정확히 보존한다.
    prior_mean_ = states_[0].x;
    prior_information_.fill(900.0);
    active_states_ = kMaxStates - 1U;
  }

  void on_relative_pose(
    const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr message)
  {
    if (message->header.frame_id != "base_previous" || active_states_ == 0U) {
      return;
    }
    shift_window_if_full();

    const std::size_t index = active_states_;
    states_[index] = State{};
    states_[index].valid = true;
    states_[index].stamp = message->header.stamp;

    const std::array<double, 3> delta_body{
      message->pose.pose.position.x,
      message->pose.pose.position.y,
      message->pose.pose.position.z};
    const auto delta_world = previous_body_to_world(delta_body, states_[index - 1U].x);
    const auto delta_rpy = rpy_from_quaternion(message->pose.pose.orientation);
    states_[index].relative = {
      delta_world[0], delta_world[1], delta_world[2],
      delta_rpy[0], delta_rpy[1], delta_rpy[2]};
    fill_information(*message, states_[index].relative_information, 0.01 * 0.01);

    // 새 state의 초기값은 x_i = x_{i-1} + z_relative. 이후 factor들이 이를 함께 보정한다.
    for (std::size_t d = 0U; d < kPoseDoF; ++d) {
      states_[index].x[d] = states_[index - 1U].x[d] + states_[index].relative[d];
    }
    ++active_states_;
    ++sequence_;

    for (auto & pending : pending_absolute_) {
      if (pending.valid && add_absolute_to_existing_state(pending.message)) {
        pending.valid = false;
      }
    }
    solve_and_publish();
  }

  void add_diagonal_factor(
    const std::size_t state_index, const std::array<double, kPoseDoF> & measurement,
    const std::array<double, kPoseDoF> & information)
  {
    for (std::size_t d = 0U; d < kPoseDoF; ++d) {
      const std::size_t row = state_index * kPoseDoF + d;
      const double residual = states_[state_index].x[d] - measurement[d];
      hessian_[row * kMaxDimension + row] += information[d];
      gradient_[row] += information[d] * residual;
    }
  }

  void build_normal_equations(const std::size_t dimension)
  {
    std::fill(hessian_.begin(), hessian_.end(), 0.0);
    std::fill(gradient_.begin(), gradient_.end(), 0.0);

    add_diagonal_factor(0U, prior_mean_, prior_information_);
    for (std::size_t i = 0U; i < active_states_; ++i) {
      if (states_[i].has_absolute) {
        add_diagonal_factor(i, states_[i].absolute, states_[i].absolute_information);
      }
      if (i == 0U) {
        continue;
      }
      for (std::size_t d = 0U; d < kPoseDoF; ++d) {
        const std::size_t previous = (i - 1U) * kPoseDoF + d;
        const std::size_t current = i * kPoseDoF + d;
        const double weight = states_[i].relative_information[d];
        // r = (x_i - x_{i-1}) - z, J=[-I, I], 따라서 H=J^T W J, b=J^T W r다.
        const double residual = states_[i].x[d] - states_[i - 1U].x[d] -
          states_[i].relative[d];
        hessian_[previous * kMaxDimension + previous] += weight;
        hessian_[current * kMaxDimension + current] += weight;
        hessian_[previous * kMaxDimension + current] -= weight;
        hessian_[current * kMaxDimension + previous] -= weight;
        gradient_[previous] -= weight * residual;
        gradient_[current] += weight * residual;
      }
    }
    // 작은 Levenberg damping은 수치적 rank 부족에서 pivot이 0이 되는 것을 막는다.
    for (std::size_t i = 0U; i < dimension; ++i) {
      hessian_[i * kMaxDimension + i] += 1.0e-8;
    }
  }

  bool solve_linear_system(const std::size_t dimension)
  {
    solve_matrix_ = hessian_;
    for (std::size_t i = 0U; i < dimension; ++i) {
      solve_rhs_[i] = -gradient_[i];
    }

    // 부분 피벗 Gaussian elimination. 차원은 최대 48로 고정되어 실행 상한을 셀 수 있다.
    for (std::size_t column = 0U; column < dimension; ++column) {
      std::size_t pivot = column;
      double pivot_magnitude = std::abs(solve_matrix_[column * kMaxDimension + column]);
      for (std::size_t row = column + 1U; row < dimension; ++row) {
        const double candidate = std::abs(solve_matrix_[row * kMaxDimension + column]);
        if (candidate > pivot_magnitude) {
          pivot = row;
          pivot_magnitude = candidate;
        }
      }
      if (!std::isfinite(pivot_magnitude) || pivot_magnitude < 1.0e-12) {
        return false;
      }
      if (pivot != column) {
        for (std::size_t c = column; c < dimension; ++c) {
          std::swap(
            solve_matrix_[column * kMaxDimension + c],
            solve_matrix_[pivot * kMaxDimension + c]);
        }
        std::swap(solve_rhs_[column], solve_rhs_[pivot]);
      }
      for (std::size_t row = column + 1U; row < dimension; ++row) {
        const double factor = solve_matrix_[row * kMaxDimension + column] /
          solve_matrix_[column * kMaxDimension + column];
        solve_matrix_[row * kMaxDimension + column] = 0.0;
        for (std::size_t c = column + 1U; c < dimension; ++c) {
          solve_matrix_[row * kMaxDimension + c] -=
            factor * solve_matrix_[column * kMaxDimension + c];
        }
        solve_rhs_[row] -= factor * solve_rhs_[column];
      }
    }

    for (std::size_t reverse = dimension; reverse > 0U; --reverse) {
      const std::size_t row = reverse - 1U;
      double value = solve_rhs_[row];
      for (std::size_t c = row + 1U; c < dimension; ++c) {
        value -= solve_matrix_[row * kMaxDimension + c] * delta_[c];
      }
      delta_[row] = value / solve_matrix_[row * kMaxDimension + row];
      if (!std::isfinite(delta_[row])) {
        return false;
      }
    }
    return true;
  }

  bool optimize()
  {
    const std::size_t dimension = active_states_ * kPoseDoF;
    for (std::size_t iteration = 0U; iteration < kGaussNewtonIterations; ++iteration) {
      build_normal_equations(dimension);
      if (!solve_linear_system(dimension)) {
        return false;
      }
      for (std::size_t i = 0U; i < active_states_; ++i) {
        for (std::size_t d = 0U; d < kPoseDoF; ++d) {
          states_[i].x[d] += delta_[i * kPoseDoF + d];
        }
      }
    }
    // 마지막 선형화점의 H diagonal을 covariance 근사에 쓰기 위해 한 번 다시 구성한다.
    build_normal_equations(dimension);
    return true;
  }

  void solve_and_publish()
  {
    if (active_states_ == 0U) {
      return;
    }
    const auto begin = std::chrono::steady_clock::now();
    const bool solved = optimize();
    const auto elapsed = std::chrono::duration<double, std::micro>(
      std::chrono::steady_clock::now() - begin).count();
    max_solve_microseconds_ = std::max(max_solve_microseconds_, elapsed);
    if (!solved) {
      ++solve_failures_;
      return;
    }

    const std::size_t latest = active_states_ - 1U;
    geometry_msgs::msg::PoseWithCovarianceStamped output;
    output.header.stamp = states_[latest].stamp;
    output.header.frame_id = "map";
    output.pose.pose.position.x = states_[latest].x[0];
    output.pose.pose.position.y = states_[latest].x[1];
    output.pose.pose.position.z = states_[latest].x[2];
    output.pose.pose.orientation = quaternion_from_rpy(
      states_[latest].x[3], states_[latest].x[4], states_[latest].x[5]);
    output.pose.covariance.fill(0.0);
    constexpr std::array<std::size_t, kPoseDoF> diagonal_indices{0U, 7U, 14U, 21U, 28U, 35U};
    for (std::size_t d = 0U; d < kPoseDoF; ++d) {
      const std::size_t row = latest * kPoseDoF + d;
      // 1/H_ii는 full inverse가 아닌 보수적이지 않을 수 있는 교육용 diagonal 근사다.
      output.pose.covariance[diagonal_indices[d]] = 1.0 /
        std::max(hessian_[row * kMaxDimension + row], 1.0e-9);
    }
    pose_pub_->publish(output);

    daily_robotics_2026_10_20::msg::FusionStats stats;
    stats.stamp = states_[latest].stamp;
    stats.sequence = sequence_;
    stats.active_states = static_cast<std::uint8_t>(active_states_);
    stats.linear_dimension = static_cast<std::uint8_t>(active_states_ * kPoseDoF);
    stats.gauss_newton_iterations = static_cast<std::uint8_t>(kGaussNewtonIterations);
    stats.pending_absolute_factors = static_cast<std::uint16_t>(std::count_if(
      pending_absolute_.begin(), pending_absolute_.end(),
      [](const PendingAbsolute & pending) {return pending.valid;}));
    stats.delayed_factors_inserted = delayed_factors_inserted_;
    stats.too_old_factors_dropped = too_old_factors_dropped_;
    stats.solve_failures = solve_failures_;
    stats.last_solve_microseconds = elapsed;
    stats.max_solve_microseconds = max_solve_microseconds_;
    stats_pub_->publish(stats);
  }

  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr relative_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr absolute_sub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pose_pub_;
  rclcpp::Publisher<daily_robotics_2026_10_20::msg::FusionStats>::SharedPtr stats_pub_;

  std::array<State, kMaxStates> states_{};
  std::array<PendingAbsolute, kMaxStates> pending_absolute_{};
  std::array<double, kPoseDoF> prior_mean_{};
  std::array<double, kPoseDoF> prior_information_{};
  std::array<double, kMaxDimension * kMaxDimension> hessian_{};
  std::array<double, kMaxDimension * kMaxDimension> solve_matrix_{};
  std::array<double, kMaxDimension> gradient_{};
  std::array<double, kMaxDimension> solve_rhs_{};
  std::array<double, kMaxDimension> delta_{};
  std::size_t active_states_{0U};
  std::uint32_t sequence_{0U};
  std::uint32_t delayed_factors_inserted_{0U};
  std::uint32_t too_old_factors_dropped_{0U};
  std::uint32_t solve_failures_{0U};
  double max_solve_microseconds_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor는 두 subscriber callback이 동시에 state 배열을 수정하지 못하게 한다.
  // 이는 mutex를 없애지만 OS scheduling의 hard-RT 보장까지 제공하지는 않는다.
  auto node = std::make_shared<BoundedPoseSmoother>();
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
