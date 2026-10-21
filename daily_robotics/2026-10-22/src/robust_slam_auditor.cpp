#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "daily_robotics_2026_10_22/msg/optimization_stats.hpp"
#include "daily_robotics_2026_10_22/msg/pose_graph_input.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
bool same_stamp(const builtin_interfaces::msg::Time & lhs, const builtin_interfaces::msg::Time & rhs)
{
  return lhs.sec == rhs.sec && lhs.nanosec == rhs.nanosec;
}

double position_rmse(
  const nav_msgs::msg::Path & path,
  const daily_robotics_2026_10_22::msg::PoseGraphInput & input)
{
  if (path.poses.size() != static_cast<std::size_t>(input.pose_count) || input.pose_count == 0U) {
    return std::numeric_limits<double>::infinity();
  }
  double squared_error_sum = 0.0;
  for (std::size_t pose = 0U; pose < path.poses.size(); ++pose) {
    const double dx = path.poses[pose].pose.position.x - input.truth_x[pose];
    const double dy = path.poses[pose].pose.position.y - input.truth_y[pose];
    squared_error_sum += dx * dx + dy * dy;
  }
  return std::sqrt(squared_error_sum / static_cast<double>(path.poses.size()));
}
}  // namespace

class RobustSlamAuditor : public rclcpp::Node
{
public:
  RobustSlamAuditor()
  : Node("robust_slam_auditor")
  {
    const auto snapshot_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    const auto result_qos = rclcpp::QoS(rclcpp::KeepLast(5)).reliable();
    // pass도 transient_local로 보관해 smoke-test 구독자가 조금 늦게 붙어도 결과를 받게 한다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(1).reliable().transient_local());

    input_subscription_ = create_subscription<daily_robotics_2026_10_22::msg::PoseGraphInput>(
      "/study/pose_graph_input", snapshot_qos,
      [this](daily_robotics_2026_10_22::msg::PoseGraphInput::SharedPtr message) {
        input_ = std::move(message);
        try_audit();
      });
    robust_subscription_ = create_subscription<nav_msgs::msg::Path>(
      "/study/robust_path", result_qos,
      [this](nav_msgs::msg::Path::SharedPtr message) {
        robust_path_ = std::move(message);
        try_audit();
      });
    naive_subscription_ = create_subscription<nav_msgs::msg::Path>(
      "/study/naive_path", result_qos,
      [this](nav_msgs::msg::Path::SharedPtr message) {
        naive_path_ = std::move(message);
        try_audit();
      });
    stats_subscription_ =
      create_subscription<daily_robotics_2026_10_22::msg::OptimizationStats>(
      "/study/optimization_stats", result_qos,
      [this](daily_robotics_2026_10_22::msg::OptimizationStats::SharedPtr message) {
        stats_ = std::move(message);
        try_audit();
      });
  }

private:
  void try_audit()
  {
    if (!input_ || !robust_path_ || !naive_path_ || !stats_) {
      return;
    }
    // 토픽 도착 순서는 DDS가 보장하지 않는다. Header.stamp가 모두 같은 graph snapshot만 묶는다.
    if (!same_stamp(input_->header.stamp, robust_path_->header.stamp) ||
      !same_stamp(input_->header.stamp, naive_path_->header.stamp) ||
      !same_stamp(input_->header.stamp, stats_->header.stamp) ||
      input_->sequence != stats_->sequence || input_->sequence == last_audited_sequence_)
    {
      return;
    }
    last_audited_sequence_ = input_->sequence;

    // optimizer가 보고한 cost를 신뢰하지 않고, 발행된 Path를 정답과 다시 비교한다.
    const double robust_rmse = position_rmse(*robust_path_, *input_);
    const double naive_rmse = position_rmse(*naive_path_, *input_);
    double minimum_inlier_switch = 1.0;
    double maximum_outlier_switch = 0.0;
    std::size_t inlier_count = 0U;
    std::size_t outlier_count = 0U;
    for (std::size_t edge = 0U; edge < input_->loop_count; ++edge) {
      if (input_->loop_expected_inlier[edge]) {
        minimum_inlier_switch = std::min(minimum_inlier_switch, stats_->loop_switches[edge]);
        ++inlier_count;
      } else {
        maximum_outlier_switch = std::max(maximum_outlier_switch, stats_->loop_switches[edge]);
        ++outlier_count;
      }
    }

    // 이 조건은 알고리즘 품질과 구조적 RT 예산을 함께 본다.
    // 5 ms는 Docker 관측용 교육 예산이며 실제 하드 RT WCET 증명은 아니다.
    const bool sample_pass = stats_->solver_ok && std::isfinite(robust_rmse) &&
      std::isfinite(naive_rmse) && inlier_count >= 1U && outlier_count >= 1U &&
      robust_rmse < 0.20 && naive_rmse > robust_rmse * 1.8 &&
      minimum_inlier_switch > 0.75 && maximum_outlier_switch < 0.10 &&
      stats_->solve_time_us < 5000.0;

    if (sample_pass) {
      ++consecutive_passes_;
    } else {
      consecutive_passes_ = 0U;
    }
    max_solve_time_us_ = std::max(max_solve_time_us_, stats_->solve_time_us);
    RCLCPP_INFO(
      get_logger(),
      "AUDIT sample seq=%u pass=%s robust_rmse=%.6f naive_rmse=%.6f "
      "min_inlier_switch=%.4f max_outlier_switch=%.4f solve_us=%.3f",
      input_->sequence, sample_pass ? "true" : "false", robust_rmse, naive_rmse,
      minimum_inlier_switch, maximum_outlier_switch, stats_->solve_time_us);

    if (consecutive_passes_ >= 3U && !final_pass_published_) {
      std_msgs::msg::Bool passed;
      passed.data = true;
      pass_publisher_->publish(passed);
      final_pass_published_ = true;
      RCLCPP_INFO(
        get_logger(),
        "AUDIT PASS samples=%u robust_rmse=%.6f naive_rmse=%.6f "
        "min_inlier_switch=%.4f false_switch=%.4f max_solve_us=%.3f",
        consecutive_passes_, robust_rmse, naive_rmse, minimum_inlier_switch,
        maximum_outlier_switch, max_solve_time_us_);
    }
  }

  rclcpp::Subscription<daily_robotics_2026_10_22::msg::PoseGraphInput>::SharedPtr
    input_subscription_;
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr robust_subscription_;
  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr naive_subscription_;
  rclcpp::Subscription<daily_robotics_2026_10_22::msg::OptimizationStats>::SharedPtr
    stats_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;

  daily_robotics_2026_10_22::msg::PoseGraphInput::SharedPtr input_;
  nav_msgs::msg::Path::SharedPtr robust_path_;
  nav_msgs::msg::Path::SharedPtr naive_path_;
  daily_robotics_2026_10_22::msg::OptimizationStats::SharedPtr stats_;
  std::uint32_t last_audited_sequence_{0U};
  std::uint32_t consecutive_passes_{0U};
  double max_solve_time_us_{0.0};
  bool final_pass_published_{false};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor에서 latest message pointer는 callback 사이에만 바뀌므로 추가 lock이 없다.
  rclcpp::spin(std::make_shared<RobustSlamAuditor>());
  rclcpp::shutdown();
  return 0;
}
