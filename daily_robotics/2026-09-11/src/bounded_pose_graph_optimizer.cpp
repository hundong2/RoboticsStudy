#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{
// 학습 예제의 계산량/메모리 상한이다. 첫 pose는 gauge freedom 제거를 위해 고정한다.
constexpr std::size_t kMaxPoses = 13;
constexpr std::size_t kPoseDof = 3;
constexpr std::size_t kMaxVariables = (kMaxPoses - 1U) * kPoseDof;
constexpr std::size_t kMaxEdges = kMaxPoses;
constexpr std::size_t kIterations = 8;

using DenseMatrix = std::array<double, kMaxVariables * kMaxVariables>;
using DenseVector = std::array<double, kMaxVariables>;

struct Pose2
{
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
};

struct Edge
{
  std::size_t from{0U};
  std::size_t to{0U};
  Pose2 measurement{};
  // 정보행렬 Ω를 대각 성분만 보관한다. 값이 클수록 해당 residual을 강하게 신뢰한다.
  std::array<double, 3> information{{1.0, 1.0, 1.0}};
};

double normalize_angle(double angle)
{
  return std::atan2(std::sin(angle), std::cos(angle));
}

// quaternion의 평면 yaw를 복원한다. 일반 quaternion의 yaw 공식에서 x,y 항도 포함해 안전하게 계산한다.
double quaternion_to_yaw(const geometry_msgs::msg::Quaternion & q)
{
  const double sin_yaw = 2.0 * (q.w * q.z + q.x * q.y);
  const double cos_yaw = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
  return std::atan2(sin_yaw, cos_yaw);
}

// pose i 좌표계에서 본 pose j의 상대변환 h(x_i, x_j)=T_i^{-1}T_j를 계산한다.
Pose2 relative_pose(const Pose2 & from, const Pose2 & to)
{
  const double dx = to.x - from.x;
  const double dy = to.y - from.y;
  const double c = std::cos(from.yaw);
  const double s = std::sin(from.yaw);
  return Pose2{
    c * dx + s * dy,
    -s * dx + c * dy,
    normalize_angle(to.yaw - from.yaw)};
}

double closure_gap(const std::array<Pose2, kMaxPoses> & poses, std::size_t count)
{
  const double dx = poses[count - 1U].x - poses[0].x;
  const double dy = poses[count - 1U].y - poses[0].y;
  return std::hypot(dx, dy);
}
}  // namespace

// 이 노드는 odometry Path를 pose graph로 바꾸고, loop-closure 제약을 포함한
// 고정 크기 Gauss-Newton 최적화를 수행해 drift가 줄어든 /slam/optimized_path를 발행한다.
class BoundedPoseGraphOptimizer : public rclcpp::Node
{
public:
  BoundedPoseGraphOptimizer()
  : Node("bounded_pose_graph_optimizer")
  {
    const auto graph_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();

    // std::bind의 _1은 subscription이 전달하는 SharedPtr 첫 인자를 멤버 콜백에 연결한다.
    using std::placeholders::_1;
    path_subscription_ = create_subscription<nav_msgs::msg::Path>(
      "slam/raw_path", graph_qos,
      std::bind(&BoundedPoseGraphOptimizer::on_path, this, _1));
    optimized_publisher_ = create_publisher<nav_msgs::msg::Path>("slam/optimized_path", graph_qos);

    // 진단 문자열은 최적화 hot path가 끝난 뒤 만들며, 사람이 보는 최신 상태 하나만 유지한다.
    stats_publisher_ = create_publisher<std_msgs::msg::String>(
      "slam/optimizer_stats", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());
    RCLCPP_INFO(get_logger(), "bounded SE(2) pose-graph optimizer ready (max_poses=%zu)", kMaxPoses);
  }

private:
  // Matrix 원소 접근을 최대 stride로 고정하면 실제 pose 수가 바뀌어도 heap 할당이 필요 없다.
  static double & at(DenseMatrix & matrix, std::size_t row, std::size_t column)
  {
    return matrix[row * kMaxVariables + column];
  }

  static double at(const DenseMatrix & matrix, std::size_t row, std::size_t column)
  {
    return matrix[row * kMaxVariables + column];
  }

  // 첫 pose는 고정되므로 pose 1부터 최적화 변수 벡터 [x1,y1,yaw1,...]에 들어간다.
  static std::size_t variable_base(std::size_t pose_index)
  {
    return (pose_index - 1U) * kPoseDof;
  }

  // H delta = rhs를 heap 없이 Cholesky 분해로 푼다.
  // pose chain이 anchor에 연결되고 damping>0이면 H는 양의 정부호가 되어야 한다.
  static bool solve_cholesky(
    const DenseMatrix & hessian, const DenseVector & rhs,
    std::size_t dimension, DenseVector & delta)
  {
    DenseMatrix lower{};
    DenseVector intermediate{};

    for (std::size_t row = 0; row < dimension; ++row) {
      for (std::size_t column = 0; column <= row; ++column) {
        double value = at(hessian, row, column);
        for (std::size_t k = 0; k < column; ++k) {
          value -= at(lower, row, k) * at(lower, column, k);
        }
        if (row == column) {
          if (value <= 1e-12 || !std::isfinite(value)) {
            return false;
          }
          at(lower, row, column) = std::sqrt(value);
        } else {
          at(lower, row, column) = value / at(lower, column, column);
        }
      }
    }

    // 전진 대입: L y = rhs.
    for (std::size_t row = 0; row < dimension; ++row) {
      double value = rhs[row];
      for (std::size_t column = 0; column < row; ++column) {
        value -= at(lower, row, column) * intermediate[column];
      }
      intermediate[row] = value / at(lower, row, row);
    }

    // 후진 대입: L^T delta = y. size_t underflow를 피하려고 reverse offset을 사용한다.
    for (std::size_t offset = 0; offset < dimension; ++offset) {
      const std::size_t row = dimension - 1U - offset;
      double value = intermediate[row];
      for (std::size_t column = row + 1U; column < dimension; ++column) {
        value -= at(lower, column, row) * delta[column];
      }
      delta[row] = value / at(lower, row, row);
    }
    return true;
  }

  // 한 edge의 e^T Ω e, gradient J^T Ω e, Hessian 근사 J^T Ω J를 누적한다.
  static double accumulate_edge(
    const Edge & edge, const std::array<Pose2, kMaxPoses> & poses,
    DenseMatrix & hessian, DenseVector & gradient)
  {
    const Pose2 prediction = relative_pose(poses[edge.from], poses[edge.to]);
    const std::array<double, 3> residual{{
      prediction.x - edge.measurement.x,
      prediction.y - edge.measurement.y,
      normalize_angle(prediction.yaw - edge.measurement.yaw)}};

    const double c = std::cos(poses[edge.from].yaw);
    const double s = std::sin(poses[edge.from].yaw);
    // J_i = d(T_i^-1 T_j)/d[x_i,y_i,yaw_i]. 세 번째 열이 회전-병진 결합을 나타낸다.
    const std::array<std::array<double, 3>, 3> jacobian_from{{
      {{-c, -s, prediction.y}},
      {{s, -c, -prediction.x}},
      {{0.0, 0.0, -1.0}}}};
    // J_j = d(T_i^-1 T_j)/d[x_j,y_j,yaw_j].
    const std::array<std::array<double, 3>, 3> jacobian_to{{
      {{c, s, 0.0}},
      {{-s, c, 0.0}},
      {{0.0, 0.0, 1.0}}}};

    double cost = 0.0;
    for (std::size_t row = 0; row < kPoseDof; ++row) {
      cost += edge.information[row] * residual[row] * residual[row];
    }

    // anchor인 pose 0에는 변수 인덱스가 없으므로 active=false로 두고 해당 block을 건너뛴다.
    const std::array<bool, 2> active{{edge.from != 0U, edge.to != 0U}};
    const std::array<std::size_t, 2> bases{{
      edge.from == 0U ? 0U : variable_base(edge.from),
      edge.to == 0U ? 0U : variable_base(edge.to)}};
    const std::array<const std::array<std::array<double, 3>, 3> *, 2> jacobians{{
      &jacobian_from, &jacobian_to}};

    for (std::size_t side_a = 0; side_a < 2U; ++side_a) {
      if (!active[side_a]) {
        continue;
      }
      for (std::size_t column_a = 0; column_a < kPoseDof; ++column_a) {
        double gradient_value = 0.0;
        for (std::size_t residual_row = 0; residual_row < kPoseDof; ++residual_row) {
          gradient_value += (*jacobians[side_a])[residual_row][column_a] *
            edge.information[residual_row] * residual[residual_row];
        }
        gradient[bases[side_a] + column_a] += gradient_value;

        for (std::size_t side_b = 0; side_b < 2U; ++side_b) {
          if (!active[side_b]) {
            continue;
          }
          for (std::size_t column_b = 0; column_b < kPoseDof; ++column_b) {
            double hessian_value = 0.0;
            for (std::size_t residual_row = 0; residual_row < kPoseDof; ++residual_row) {
              hessian_value += (*jacobians[side_a])[residual_row][column_a] *
                edge.information[residual_row] *
                (*jacobians[side_b])[residual_row][column_b];
            }
            at(
              hessian, bases[side_a] + column_a,
              bases[side_b] + column_b) += hessian_value;
          }
        }
      }
    }
    return cost;
  }

  // 고정 8회 Gauss-Newton으로 pose를 갱신한다. 반환값은 마지막 선형화 지점의 비용이다.
  static double optimize(
    std::array<Pose2, kMaxPoses> & poses,
    const std::array<Edge, kMaxEdges> & edges,
    std::size_t pose_count, std::size_t edge_count,
    bool & solver_ok)
  {
    const std::size_t dimension = (pose_count - 1U) * kPoseDof;
    double last_cost = std::numeric_limits<double>::infinity();
    solver_ok = true;

    for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
      DenseMatrix hessian{};
      DenseVector gradient{};
      DenseVector rhs{};
      DenseVector delta{};
      last_cost = 0.0;

      for (std::size_t edge_index = 0; edge_index < edge_count; ++edge_index) {
        last_cost += accumulate_edge(edges[edge_index], poses, hessian, gradient);
      }
      for (std::size_t diagonal = 0; diagonal < dimension; ++diagonal) {
        // 작은 Levenberg식 damping은 수치적으로 거의 singular한 방향을 안정화한다.
        at(hessian, diagonal, diagonal) += 1e-6;
        // Gauss-Newton 정상방정식은 H delta = -g 이므로 부호를 뒤집는다.
        rhs[diagonal] = -gradient[diagonal];
      }
      if (!solve_cholesky(hessian, rhs, dimension, delta)) {
        solver_ok = false;
        return last_cost;
      }

      for (std::size_t pose_index = 1U; pose_index < pose_count; ++pose_index) {
        const std::size_t base = variable_base(pose_index);
        poses[pose_index].x += delta[base];
        poses[pose_index].y += delta[base + 1U];
        poses[pose_index].yaw = normalize_angle(poses[pose_index].yaw + delta[base + 2U]);
      }
    }
    return last_cost;
  }

  // 경과시간을 6개 고정 bin에 넣는다. push_back/map을 쓰지 않아 계측 상태가 동적 할당되지 않는다.
  static std::size_t histogram_bin(std::int64_t microseconds)
  {
    constexpr std::array<std::int64_t, 5> upper_bounds{{100, 250, 500, 1000, 2000}};
    for (std::size_t index = 0; index < upper_bounds.size(); ++index) {
      if (microseconds <= upper_bounds[index]) {
        return index;
      }
    }
    return upper_bounds.size();
  }

  // raw Path 수신부터 검증, graph 구성, 최적화, 결과 발행까지 담당하는 핵심 callback이다.
  void on_path(const nav_msgs::msg::Path::SharedPtr message)
  {
    const auto callback_start_steady = std::chrono::steady_clock::now();
    // 같은 ROS clock의 publisher stamp와 now 차이는 전송+대기+dispatch 지연의 근사치다.
    const auto queue_delay_ns = (now() - rclcpp::Time(message->header.stamp)).nanoseconds();
    const std::int64_t queue_delay_us = std::max<std::int64_t>(0, queue_delay_ns / 1000);

    const std::size_t pose_count = message->poses.size();
    if (pose_count < 3U || pose_count > kMaxPoses) {
      RCLCPP_WARN(
        get_logger(), "reject Path: pose_count=%zu, contract=[3,%zu]", pose_count, kMaxPoses);
      return;
    }

    std::array<Pose2, kMaxPoses> poses{};
    for (std::size_t index = 0; index < pose_count; ++index) {
      const auto & source = message->poses[index].pose;
      poses[index] = Pose2{source.position.x, source.position.y, quaternion_to_yaw(source.orientation)};
    }
    const double raw_gap = closure_gap(poses, pose_count);

    std::array<Edge, kMaxEdges> edges{};
    std::size_t edge_count = 0U;
    for (std::size_t index = 0; index + 1U < pose_count; ++index) {
      // 인접 odometry 측정 z_ij는 초기 raw pose의 T_i^-1 T_j에서 만든다.
      edges[edge_count++] = Edge{
        index, index + 1U, relative_pose(poses[index], poses[index + 1U]),
        {{200.0, 200.0, 120.0}}};
    }
    // 마지막 keyframe이 첫 장소를 재관측했다는 loop closure z=[0,0,0]을 강하게 추가한다.
    edges[edge_count++] = Edge{
      pose_count - 1U, 0U, Pose2{0.0, 0.0, 0.0}, {{800.0, 800.0, 500.0}}};

    bool solver_ok = false;
    const double final_cost = optimize(poses, edges, pose_count, edge_count, solver_ok);
    if (!solver_ok) {
      RCLCPP_ERROR(get_logger(), "Cholesky solve failed; optimized path not published");
      return;
    }

    nav_msgs::msg::Path optimized;
    optimized.header.stamp = now();
    // graph correction이 만든 전역적으로 일관된 좌표계를 학습용 map frame으로 구분한다.
    optimized.header.frame_id = "map";
    optimized.poses.reserve(pose_count);
    for (std::size_t index = 0; index < pose_count; ++index) {
      geometry_msgs::msg::PoseStamped pose;
      pose.header = optimized.header;
      pose.pose.position.x = poses[index].x;
      pose.pose.position.y = poses[index].y;
      pose.pose.position.z = 0.0;
      // yaw -> quaternion: q_z=sin(yaw/2), q_w=cos(yaw/2).
      pose.pose.orientation.z = std::sin(0.5 * poses[index].yaw);
      pose.pose.orientation.w = std::cos(0.5 * poses[index].yaw);
      optimized.poses.push_back(pose);
    }
    optimized_publisher_->publish(optimized);

    const auto algorithm_end_steady = std::chrono::steady_clock::now();
    const auto execution_us = std::chrono::duration_cast<std::chrono::microseconds>(
      algorithm_end_steady - callback_start_steady).count();
    ++callback_count_;
    max_queue_delay_us_ = std::max(max_queue_delay_us_, queue_delay_us);
    max_execution_us_ = std::max(max_execution_us_, execution_us);
    ++execution_histogram_[histogram_bin(execution_us)];

    // ostringstream/String은 동적 할당 가능성이 있으므로 측정 구간 뒤의 비-RT 진단 경로에 둔다.
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4)
           << "poses=" << pose_count
           << " edges=" << edge_count
           << " iterations=" << kIterations
           << " raw_gap_m=" << raw_gap
           << " optimized_gap_m=" << closure_gap(poses, pose_count)
           << " final_cost=" << final_cost
           << " queue_us=" << queue_delay_us
           << " exec_us=" << execution_us
           << " max_queue_us=" << max_queue_delay_us_
           << " max_exec_us=" << max_execution_us_
           << " hist_le_100_250_500_1000_2000_gt=";
    for (std::size_t index = 0; index < execution_histogram_.size(); ++index) {
      stream << (index == 0U ? "" : "/") << execution_histogram_[index];
    }
    std_msgs::msg::String stats;
    stats.data = stream.str();
    stats_publisher_->publish(stats);
  }

  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr path_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr optimized_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr stats_publisher_;
  std::uint64_t callback_count_{0U};
  std::int64_t max_queue_delay_us_{0};
  std::int64_t max_execution_us_{0};
  std::array<std::uint64_t, 6> execution_histogram_{};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor 기반 spin이라 이 노드 내부 callback은 동시에 실행되지 않는다.
  // 따라서 histogram/max 통계에는 mutex가 필요 없지만, 다른 callback을 추가하면 재검토해야 한다.
  rclcpp::spin(std::make_shared<BoundedPoseGraphOptimizer>());
  rclcpp::shutdown();
  return 0;
}
