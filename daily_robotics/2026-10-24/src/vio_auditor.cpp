#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

#include "daily_robotics_2026_10_24/msg/preintegration_stats.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
constexpr std::size_t kRingCapacity = 256U;
constexpr std::uint32_t kRequiredSamples = 120U;
constexpr double kTrueGyroBias = 0.015;

double wrap_angle(double angle)
{
  return std::atan2(std::sin(angle), std::cos(angle));
}

double yaw_from_quaternion(const geometry_msgs::msg::Quaternion & q)
{
  return std::atan2(
    2.0 * (q.w * q.z + q.x * q.y),
    1.0 - 2.0 * (q.y * q.y + q.z * q.z));
}

std::int64_t stamp_ns(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1000000000LL +
         static_cast<std::int64_t>(stamp.nanosec);
}

struct TruthEntry
{
  std::int64_t stamp{0};
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
  bool valid{false};
  bool evaluated{false};
};

struct EstimateEntry
{
  std::int64_t stamp{0};
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
  bool valid{false};
};

struct StatsEntry
{
  std::int64_t stamp{0};
  daily_robotics_2026_10_24::msg::PreintegrationStats value{};
  bool valid{false};
};
}  // namespace

class VioAuditor : public rclcpp::Node
{
public:
  VioAuditor()
  : Node("vio_auditor")
  {
    const auto reliable_qos = rclcpp::QoS(rclcpp::KeepLast(20)).reliable();
    truth_subscription_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/sim/truth_pose", reliable_qos,
      std::bind(&VioAuditor::on_truth, this, std::placeholders::_1));
    odometry_subscription_ = create_subscription<nav_msgs::msg::Odometry>(
      "/vio/odometry", reliable_qos,
      std::bind(&VioAuditor::on_odometry, this, std::placeholders::_1));
    stats_subscription_ = create_subscription<daily_robotics_2026_10_24::msg::PreintegrationStats>(
      "/vio/stats", reliable_qos,
      std::bind(&VioAuditor::on_stats, this, std::placeholders::_1));

    // Transient Local은 smoke test가 늦게 구독해도 마지막 pass 판정을 즉시 받을 수 있게 한다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(1).reliable().transient_local());
  }

private:
  void on_truth(const geometry_msgs::msg::PoseStamped::SharedPtr message)
  {
    TruthEntry & entry = truth_[truth_write_ % kRingCapacity];
    entry.stamp = stamp_ns(message->header.stamp);
    entry.x = message->pose.position.x;
    entry.y = message->pose.position.y;
    entry.yaw = yaw_from_quaternion(message->pose.orientation);
    entry.valid = true;
    entry.evaluated = false;
    ++truth_write_;
    evaluate_matches();
  }

  void on_odometry(const nav_msgs::msg::Odometry::SharedPtr message)
  {
    EstimateEntry & entry = estimates_[estimate_write_ % kRingCapacity];
    entry.stamp = stamp_ns(message->header.stamp);
    entry.x = message->pose.pose.position.x;
    entry.y = message->pose.pose.position.y;
    entry.yaw = yaw_from_quaternion(message->pose.pose.orientation);
    entry.valid = true;
    ++estimate_write_;
    evaluate_matches();
  }

  void on_stats(
    const daily_robotics_2026_10_24::msg::PreintegrationStats::SharedPtr message)
  {
    StatsEntry & entry = stats_[stats_write_ % kRingCapacity];
    entry.stamp = stamp_ns(message->header.stamp);
    entry.value = *message;
    entry.valid = true;
    ++stats_write_;
    evaluate_matches();
  }

  const EstimateEntry * find_estimate(std::int64_t stamp) const
  {
    for (const EstimateEntry & entry : estimates_) {
      if (entry.valid && entry.stamp == stamp) {
        return &entry;
      }
    }
    return nullptr;
  }

  const StatsEntry * find_stats(std::int64_t stamp) const
  {
    for (const StatsEntry & entry : stats_) {
      if (entry.valid && entry.stamp == stamp) {
        return &entry;
      }
    }
    return nullptr;
  }

  void evaluate_matches()
  {
    for (TruthEntry & truth : truth_) {
      if (!truth.valid || truth.evaluated) {
        continue;
      }
      const EstimateEntry * estimate = find_estimate(truth.stamp);
      const StatsEntry * stats = find_stats(truth.stamp);
      if (estimate == nullptr || stats == nullptr) {
        continue;
      }

      // auditor는 estimator 내부 residual이 아니라 독립 truth를 같은 measurement stamp로 매칭한다.
      const double dx = estimate->x - truth.x;
      const double dy = estimate->y - truth.y;
      const double position_error = std::hypot(dx, dy);
      const double yaw_error = std::abs(wrap_angle(estimate->yaw - truth.yaw));
      sum_position_squared_ += position_error * position_error;
      sum_yaw_squared_ += yaw_error * yaw_error;
      max_position_error_ = std::max(max_position_error_, position_error);
      max_yaw_error_ = std::max(max_yaw_error_, yaw_error);
      ++matched_samples_;
      truth.evaluated = true;
      latest_stats_ = stats->value;

      if (!published_pass_ && matched_samples_ >= kRequiredSamples) {
        publish_verdict();
      }
    }
  }

  void publish_verdict()
  {
    const double position_rmse = std::sqrt(
      sum_position_squared_ / static_cast<double>(matched_samples_));
    const double yaw_rmse = std::sqrt(
      sum_yaw_squared_ / static_cast<double>(matched_samples_));
    const double bias_error = std::abs(latest_stats_.estimated_gyro_bias - kTrueGyroBias);

    const bool no_drop = latest_stats_.imu_queue_drops == 0U &&
      latest_stats_.visual_queue_drops == 0U &&
      latest_stats_.staged_imu_drops == 0U &&
      // SensorDataQoS discovery/warm-up에서 IMU가 없는 첫 interval 하나는 건너뛰되 반드시 계측한다.
      latest_stats_.stale_visual_drops <= 1U;
    const bool structural = latest_stats_.bounds_respected &&
      latest_stats_.solve_failures == 0U &&
      latest_stats_.marginalizations >= 100U &&
      latest_stats_.window_states <= 5U &&
      latest_stats_.latest_preintegrated_samples <= 12U &&
      latest_stats_.max_imu_batch <= 8U;
    const bool accurate = position_rmse < 0.060 && yaw_rmse < 0.035 &&
      max_position_error_ < 0.160 && max_yaw_error_ < 0.080 && bias_error < 0.012;
    const bool timing_observed = latest_stats_.max_kernel_us < 10000.0;
    const bool passed = no_drop && structural && accurate && timing_observed;

    std_msgs::msg::Bool verdict;
    verdict.data = passed;
    pass_publisher_->publish(verdict);
    published_pass_ = true;

    RCLCPP_INFO(
      get_logger(),
      "AUDIT %s samples=%u pos_rmse=%.6f m yaw_rmse=%.6f rad max_pos=%.6f m "
      "max_yaw=%.6f rad bias=%.6f bias_error=%.6f marginalizations=%u "
      "drops=%u/%u/%u/%u solve_failures=%u max_batch=%u max_kernel=%.3f us min_pivot=%.6e",
      passed ? "PASS" : "FAIL", matched_samples_, position_rmse, yaw_rmse,
      max_position_error_, max_yaw_error_, latest_stats_.estimated_gyro_bias, bias_error,
      latest_stats_.marginalizations, latest_stats_.imu_queue_drops,
      latest_stats_.visual_queue_drops, latest_stats_.staged_imu_drops,
      latest_stats_.stale_visual_drops, latest_stats_.solve_failures,
      latest_stats_.max_imu_batch, latest_stats_.max_kernel_us,
      latest_stats_.minimum_marginal_pivot);
  }

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr truth_subscription_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometry_subscription_;
  rclcpp::Subscription<daily_robotics_2026_10_24::msg::PreintegrationStats>::SharedPtr
    stats_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;

  std::array<TruthEntry, kRingCapacity> truth_{};
  std::array<EstimateEntry, kRingCapacity> estimates_{};
  std::array<StatsEntry, kRingCapacity> stats_{};
  std::size_t truth_write_{0U};
  std::size_t estimate_write_{0U};
  std::size_t stats_write_{0U};

  std::uint32_t matched_samples_{0U};
  double sum_position_squared_{0.0};
  double sum_yaw_squared_{0.0};
  double max_position_error_{0.0};
  double max_yaw_error_{0.0};
  daily_robotics_2026_10_24::msg::PreintegrationStats latest_stats_{};
  bool published_pass_{false};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<VioAuditor>());
  rclcpp::shutdown();
  return 0;
}
