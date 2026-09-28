#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "daily_robotics_2026_09_29/msg/fusion_stats.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

// FusionAuditor의 역할:
// estimator가 보내는 자기평가만 믿지 않고 별도 ground truth로 timestamp-matched 위치 오차를
// 재계산하며, outlier 억제·window 상한·marginalization·실행시간 조건을 모두 확인한다.
class FusionAuditor : public rclcpp::Node
{
public:
  FusionAuditor()
  : Node("fusion_auditor")
  {
    truth_subscription_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/sim/ground_truth", rclcpp::QoS(20).reliable(),
      [this](const geometry_msgs::msg::PoseStamped::ConstSharedPtr message) {on_truth(message);});
    fused_subscription_ = create_subscription<nav_msgs::msg::Odometry>(
      "/fusion/odometry", rclcpp::QoS(20).reliable(),
      [this](const nav_msgs::msg::Odometry::ConstSharedPtr message) {on_fused(message);});
    stats_subscription_ =
      create_subscription<daily_robotics_2026_09_29::msg::FusionStats>(
      "/fusion/stats", rclcpp::QoS(20).reliable(),
      [this](const daily_robotics_2026_09_29::msg::FusionStats::ConstSharedPtr message) {
        on_stats(message);
      });

    // transient_local은 늦게 시작한 smoke test도 이미 발표된 PASS 값을 받을 수 있게 한다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/audit_pass", rclcpp::QoS(1).reliable().transient_local());
  }

private:
  static constexpr std::size_t kRing = 64U;

  struct Sample
  {
    std::int64_t stamp_ns{0};
    double x{0.0};
    double y{0.0};
    bool valid{false};
  };

  void on_truth(const geometry_msgs::msg::PoseStamped::ConstSharedPtr & message)
  {
    Sample & slot = truth_ring_[truth_write_ % kRing];
    slot.stamp_ns = rclcpp::Time(message->header.stamp).nanoseconds();
    slot.x = message->pose.position.x;
    slot.y = message->pose.position.y;
    slot.valid = true;
    ++truth_write_;
  }

  void on_fused(const nav_msgs::msg::Odometry::ConstSharedPtr & message)
  {
    const std::int64_t stamp = rclcpp::Time(message->header.stamp).nanoseconds();
    if (stamp == last_counted_stamp_) {
      return;
    }
    for (const Sample & truth : truth_ring_) {
      if (!truth.valid || truth.stamp_ns != stamp) {
        continue;
      }
      const double error = std::hypot(
        message->pose.pose.position.x - truth.x,
        message->pose.pose.position.y - truth.y);
      maximum_error_m_ = std::max(maximum_error_m_, error);
      error_sum_m_ += error;
      ++matched_samples_;
      last_counted_stamp_ = stamp;
      evaluate();
      return;
    }
  }

  void on_stats(const daily_robotics_2026_09_29::msg::FusionStats::ConstSharedPtr & message)
  {
    latest_stats_ = *message;
    have_stats_ = true;
    minimum_observed_weight_ = std::min(minimum_observed_weight_, message->minimum_gnss_weight);
    evaluate();
  }

  void evaluate()
  {
    if (published_ || !have_stats_ || matched_samples_ < 70U) {
      return;
    }
    const double mean_error = error_sum_m_ / static_cast<double>(matched_samples_);
    const bool pass =
      latest_stats_.window_size == 8U &&
      latest_stats_.irls_iterations == 4U &&
      latest_stats_.marginalizations >= 50U &&
      latest_stats_.gnss_associated >= 15U &&
      latest_stats_.downweighted_outliers >= 2U &&
      latest_stats_.gnss_rejected == 0U &&
      minimum_observed_weight_ < 0.2 &&
      latest_stats_.maximum_pair_skew_ms <= 60.0 &&
      latest_stats_.latest_variance_x > 0.0 &&
      latest_stats_.latest_variance_y > 0.0 &&
      latest_stats_.maximum_solve_time_us < 5000.0 &&
      mean_error < 0.45 && maximum_error_m_ < 0.90;

    if (!pass) {
      return;
    }

    std_msgs::msg::Bool result;
    result.data = true;
    pass_publisher_->publish(result);
    published_ = true;
    RCLCPP_INFO(
      get_logger(),
      "AUDIT_PASS samples=%zu mean_error_m=%.6f max_error_m=%.6f marg=%u outliers=%u "
      "min_weight=%.6f max_solve_us=%.3f variance=(%.6f,%.6f)",
      matched_samples_, mean_error, maximum_error_m_, latest_stats_.marginalizations,
      latest_stats_.downweighted_outliers, minimum_observed_weight_,
      latest_stats_.maximum_solve_time_us, latest_stats_.latest_variance_x,
      latest_stats_.latest_variance_y);
  }

  std::array<Sample, kRing> truth_ring_{};
  std::size_t truth_write_{0U};
  std::int64_t last_counted_stamp_{-1};
  std::size_t matched_samples_{0U};
  double error_sum_m_{0.0};
  double maximum_error_m_{0.0};
  double minimum_observed_weight_{1.0};
  bool have_stats_{false};
  bool published_{false};
  daily_robotics_2026_09_29::msg::FusionStats latest_stats_{};
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr truth_subscription_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr fused_subscription_;
  rclcpp::Subscription<daily_robotics_2026_09_29::msg::FusionStats>::SharedPtr
    stats_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FusionAuditor>());
  rclcpp::shutdown();
  return 0;
}
