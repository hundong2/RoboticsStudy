#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "daily_robotics_2026_10_21/msg/estimator_stats.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
constexpr std::size_t kAuditRingCapacity = 128U;

std::int64_t stamp_to_ns(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1000000000LL +
         static_cast<std::int64_t>(stamp.nanosec);
}

struct PoseEntry
{
  std::int64_t stamp_ns{0LL};
  double height_m{0.0};
  bool valid{false};
  bool matched{false};
};
}  // namespace

class ContactEstimatorAuditor : public rclcpp::Node
{
public:
  ContactEstimatorAuditor()
  : Node("contact_estimator_auditor")
  {
    const auto qos = rclcpp::QoS(rclcpp::KeepLast(20)).reliable();
    truth_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/sim/truth_pose", qos,
      std::bind(&ContactEstimatorAuditor::on_truth, this, std::placeholders::_1));
    estimate_sub_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/estimator/pose", qos,
      std::bind(&ContactEstimatorAuditor::on_estimate, this, std::placeholders::_1));
    stats_sub_ = create_subscription<daily_robotics_2026_10_21::msg::EstimatorStats>(
      "/estimator/stats", qos,
      std::bind(&ContactEstimatorAuditor::on_stats, this, std::placeholders::_1));

    // transient_local은 auditor가 먼저 PASS를 내도 늦게 시작한 smoke subscriber가 결과를 받게 한다.
    pass_pub_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
  }

private:
  void on_truth(const geometry_msgs::msg::PoseStamped::SharedPtr message)
  {
    const std::int64_t stamp_ns = stamp_to_ns(message->header.stamp);
    if (message->header.frame_id != "map" || stamp_ns <= 0LL ||
      !std::isfinite(message->pose.position.z))
    {
      ++invalid_outputs_;
      return;
    }
    PoseEntry & entry = truth_ring_[truth_write_index_];
    entry = PoseEntry{stamp_ns, message->pose.position.z, true, false};
    truth_write_index_ = (truth_write_index_ + 1U) % kAuditRingCapacity;
    try_match(stamp_ns);
  }

  void on_estimate(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr message)
  {
    const std::int64_t stamp_ns = stamp_to_ns(message->header.stamp);
    const double variance = message->pose.covariance[14];
    if (message->header.frame_id != "map" || stamp_ns <= 0LL ||
      !std::isfinite(message->pose.pose.position.z) ||
      !std::isfinite(variance) || variance <= 0.0)
    {
      ++invalid_outputs_;
      return;
    }
    latest_height_variance_ = variance;
    PoseEntry & entry = estimate_ring_[estimate_write_index_];
    entry = PoseEntry{stamp_ns, message->pose.pose.position.z, true, false};
    estimate_write_index_ = (estimate_write_index_ + 1U) % kAuditRingCapacity;
    try_match(stamp_ns);
  }

  void on_stats(const daily_robotics_2026_10_21::msg::EstimatorStats::SharedPtr message)
  {
    // estimator가 스스로 주장한 상한을 그대로 믿지 않고 아래 PASS 조건에서 다시 비교한다.
    latest_stats_ = *message;
    have_stats_ = true;
    evaluate_pass();
  }

  void try_match(const std::int64_t stamp_ns)
  {
    PoseEntry * truth = nullptr;
    PoseEntry * estimate = nullptr;
    // 두 128-slot ring의 선형 검색은 데이터와 무관하게 최대 256회 비교로 끝난다.
    for (PoseEntry & entry : truth_ring_) {
      if (entry.valid && !entry.matched && entry.stamp_ns == stamp_ns) {
        truth = &entry;
        break;
      }
    }
    for (PoseEntry & entry : estimate_ring_) {
      if (entry.valid && !entry.matched && entry.stamp_ns == stamp_ns) {
        estimate = &entry;
        break;
      }
    }
    if (truth == nullptr || estimate == nullptr) {
      return;
    }

    truth->matched = true;
    estimate->matched = true;
    const double error_m = estimate->height_m - truth->height_m;
    squared_error_sum_ += error_m * error_m;
    max_absolute_error_m_ = std::max(max_absolute_error_m_, std::abs(error_m));
    ++matched_samples_;
    evaluate_pass();
  }

  void evaluate_pass()
  {
    if (pass_published_ || !have_stats_ || matched_samples_ < 600U) {
      return;
    }
    const double rmse_m = std::sqrt(
      squared_error_sum_ / static_cast<double>(matched_samples_));
    const bool accuracy_ok = rmse_m < 0.06 && max_absolute_error_m_ < 0.16;
    const bool transitions_ok =
      latest_stats_.contact_transitions >= 4U &&
      latest_stats_.contact_updates >= 100U &&
      latest_stats_.rejected_updates >= 5U;
    const bool resource_ok =
      latest_stats_.bounded_contract_ok && latest_stats_.queue_drops == 0U &&
      latest_stats_.invalid_samples == 0U &&
      latest_stats_.max_samples_per_cycle <= 4U;
    const bool covariance_ok =
      latest_height_variance_ > 0.0 && latest_height_variance_ < 0.02;

    if (accuracy_ok && transitions_ok && resource_ok && covariance_ok && invalid_outputs_ == 0U) {
      std_msgs::msg::Bool result;
      result.data = true;
      pass_pub_->publish(result);
      pass_published_ = true;
      RCLCPP_INFO(
        get_logger(),
        "AUDIT PASS samples=%u rmse=%.6f_m max_error=%.6f_m transitions=%u "
        "updates=%u rejected=%u drops=%u invalid=%u max_batch=%u "
        "bias=%.6f_mps2 max_cycle=%.3f_us",
        matched_samples_, rmse_m, max_absolute_error_m_, latest_stats_.contact_transitions,
        latest_stats_.contact_updates, latest_stats_.rejected_updates,
        latest_stats_.queue_drops, latest_stats_.invalid_samples,
        latest_stats_.max_samples_per_cycle, latest_stats_.accel_bias_mps2,
        latest_stats_.max_cycle_time_us);
    }
  }

  std::array<PoseEntry, kAuditRingCapacity> truth_ring_{};
  std::array<PoseEntry, kAuditRingCapacity> estimate_ring_{};
  std::size_t truth_write_index_{0U};
  std::size_t estimate_write_index_{0U};
  std::uint32_t matched_samples_{0U};
  std::uint32_t invalid_outputs_{0U};
  double squared_error_sum_{0.0};
  double max_absolute_error_m_{0.0};
  double latest_height_variance_{0.0};
  bool have_stats_{false};
  bool pass_published_{false};
  daily_robotics_2026_10_21::msg::EstimatorStats latest_stats_{};

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr truth_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr estimate_sub_;
  rclcpp::Subscription<daily_robotics_2026_10_21::msg::EstimatorStats>::SharedPtr stats_sub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_pub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // 감사 노드는 RT 경로와 분리되어 있으며 SingleThreadedExecutor로 판정 순서를 단순하게 유지한다.
  rclcpp::spin(std::make_shared<ContactEstimatorAuditor>());
  rclcpp::shutdown();
  return 0;
}
