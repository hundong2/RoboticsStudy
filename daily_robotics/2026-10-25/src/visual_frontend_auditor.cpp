#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "daily_robotics_2026_10_25/msg/feature_track_stats.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
constexpr std::size_t kAuditSlots = 256U;

// 서로 다른 topic의 도착 순서를 가정하지 않고 timestamp key로 결합하는 fixed-capacity 저장소다.
template<typename MessageT, std::size_t Capacity>
class StampRing
{
public:
  void insert(const std::int64_t stamp_ns, const MessageT & message)
  {
    // 같은 stamp가 재수신되면 기존 slot을 갱신한다.
    for (auto & slot : slots_) {
      if (slot.valid && slot.stamp_ns == stamp_ns) {
        slot.message = message;
        return;
      }
    }
    slots_[next_] = Slot{stamp_ns, message, true};
    next_ = (next_ + 1U) % Capacity;
  }

  MessageT * find(const std::int64_t stamp_ns)
  {
    for (auto & slot : slots_) {
      if (slot.valid && slot.stamp_ns == stamp_ns) {
        return &slot.message;
      }
    }
    return nullptr;
  }

  void erase(const std::int64_t stamp_ns)
  {
    for (auto & slot : slots_) {
      if (slot.valid && slot.stamp_ns == stamp_ns) {
        slot.valid = false;
        return;
      }
    }
  }

private:
  struct Slot
  {
    std::int64_t stamp_ns{0};
    MessageT message{};
    bool valid{false};
  };

  std::array<Slot, Capacity> slots_{};
  std::size_t next_{0U};
};

std::int64_t stamp_to_nanoseconds(const builtin_interfaces::msg::Time & stamp)
{
  // ROS Time 생성자는 sec/nanosec 정규화와 clock-independent integer 변환을 제공한다.
  return rclcpp::Time(stamp).nanoseconds();
}
}  // namespace

class VisualFrontendAuditor final : public rclcpp::Node
{
public:
  VisualFrontendAuditor()
  : Node("visual_frontend_auditor")
  {
    // 세 subscriber는 topic 간 순서를 믿지 않는다. callback마다 ring에 넣고 같은 stamp 3종이
    // 모두 도착했을 때만 하나의 검증 sample로 확정한다.
    truth_subscription_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/sim/camera_truth", rclcpp::QoS(10).reliable(),
      [this](const geometry_msgs::msg::PoseStamped::ConstSharedPtr message) {
        const auto stamp = stamp_to_nanoseconds(message->header.stamp);
        truths_.insert(stamp, *message);
        try_audit(stamp);
      });
    odometry_subscription_ = create_subscription<nav_msgs::msg::Odometry>(
      "/visual_frontend/odometry", rclcpp::QoS(10).reliable(),
      [this](const nav_msgs::msg::Odometry::ConstSharedPtr message) {
        const auto stamp = stamp_to_nanoseconds(message->header.stamp);
        odometries_.insert(stamp, *message);
        try_audit(stamp);
      });
    stats_subscription_ =
      create_subscription<daily_robotics_2026_10_25::msg::FeatureTrackStats>(
      "/visual_frontend/stats", rclcpp::QoS(10).reliable(),
      [this](const daily_robotics_2026_10_25::msg::FeatureTrackStats::ConstSharedPtr message) {
        const auto stamp = stamp_to_nanoseconds(message->header.stamp);
        statistics_.insert(stamp, *message);
        try_audit(stamp);
      });

    // Transient Local result는 smoke subscriber가 auditor보다 늦게 시작해도 최종 pass를 받게 한다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
  }

private:
  // 동일 acquisition stamp의 truth/odometry/stats를 결합해 독립 acceptance 조건을 계산한다.
  void try_audit(const std::int64_t stamp_ns)
  {
    auto * truth = truths_.find(stamp_ns);
    auto * odometry = odometries_.find(stamp_ns);
    auto * stats = statistics_.find(stamp_ns);
    if (truth == nullptr || odometry == nullptr || stats == nullptr) {
      return;
    }

    // VO의 world origin은 임의(gauge freedom)다. 첫 exact-stamp pair에서 translation origin만 정렬하고,
    // 이후 증가량/드리프트는 전혀 보정하지 않는다.
    if (!origin_aligned_) {
      truth_minus_estimate_origin_ = truth->pose.position.x - odometry->pose.pose.position.x;
      origin_aligned_ = true;
    }

    if (stats->tracked_count > 0U) {
      const double aligned_estimate = odometry->pose.pose.position.x + truth_minus_estimate_origin_;
      const double error = aligned_estimate - truth->pose.position.x;
      squared_error_sum_ += error * error;
      max_absolute_error_ = std::max(max_absolute_error_, std::abs(error));
      max_flow_error_ = std::max(max_flow_error_, std::abs(stats->median_flow_x + 1.0));
      minimum_tracked_ = std::min(minimum_tracked_, stats->tracked_count);
      max_kernel_us_ = std::max(max_kernel_us_, stats->max_kernel_us);
      max_queue_drops_ = std::max(max_queue_drops_, stats->frame_queue_drops);
      all_bounds_respected_ = all_bounds_respected_ && stats->bounds_respected;
      all_camera_models_valid_ = all_camera_models_valid_ && stats->camera_model_valid;
      ++audited_samples_;
    }

    truths_.erase(stamp_ns);
    odometries_.erase(stamp_ns);
    statistics_.erase(stamp_ns);

    if (!result_published_ && audited_samples_ >= 90U) {
      const double rmse = std::sqrt(squared_error_sum_ / static_cast<double>(audited_samples_));
      const bool pass = rmse < 0.020 && max_absolute_error_ < 0.040 && max_flow_error_ < 0.1 &&
        minimum_tracked_ >= 20U && max_queue_drops_ == 0U &&
        all_bounds_respected_ && all_camera_models_valid_;

      std_msgs::msg::Bool result;
      result.data = pass;
      pass_publisher_->publish(result);
      result_published_ = true;

      if (pass) {
        RCLCPP_INFO(
          get_logger(),
          "AUDIT PASS samples=%u rmse=%.6f m max_error=%.6f m min_tracks=%u "
          "max_flow_error=%.3f px max_kernel=%.3f us drops=%u",
          audited_samples_, rmse, max_absolute_error_, minimum_tracked_, max_flow_error_,
          max_kernel_us_, max_queue_drops_);
      } else {
        RCLCPP_ERROR(
          get_logger(),
          "AUDIT FAIL samples=%u rmse=%.6f m max_error=%.6f m min_tracks=%u "
          "max_flow_error=%.3f px bounds=%s camera=%s drops=%u",
          audited_samples_, rmse, max_absolute_error_, minimum_tracked_, max_flow_error_,
          all_bounds_respected_ ? "true" : "false",
          all_camera_models_valid_ ? "true" : "false", max_queue_drops_);
      }
    }
  }

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr truth_subscription_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometry_subscription_;
  rclcpp::Subscription<daily_robotics_2026_10_25::msg::FeatureTrackStats>::SharedPtr
    stats_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;

  StampRing<geometry_msgs::msg::PoseStamped, kAuditSlots> truths_;
  StampRing<nav_msgs::msg::Odometry, kAuditSlots> odometries_;
  StampRing<daily_robotics_2026_10_25::msg::FeatureTrackStats, kAuditSlots> statistics_;

  bool origin_aligned_{false};
  bool result_published_{false};
  bool all_bounds_respected_{true};
  bool all_camera_models_valid_{true};
  double truth_minus_estimate_origin_{0.0};
  double squared_error_sum_{0.0};
  double max_absolute_error_{0.0};
  double max_flow_error_{0.0};
  double max_kernel_us_{0.0};
  std::uint32_t minimum_tracked_{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t max_queue_drops_{0U};
  std::uint32_t audited_samples_{0U};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // auditor는 작은 callback만 수행하므로 SingleThreaded spin으로 state ownership을 단순화한다.
  rclcpp::spin(std::make_shared<VisualFrontendAuditor>());
  rclcpp::shutdown();
  return 0;
}
