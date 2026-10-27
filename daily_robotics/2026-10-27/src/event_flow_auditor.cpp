#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "daily_robotics_2026_10_27/msg/event_motion_stats.hpp"
#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
constexpr std::size_t kAuditSlots = 256U;

// 두 topic의 DDS 도착 순서를 가정하지 않고 acquisition stamp로 결합하는 fixed-capacity 저장소다.
template<typename MessageT, std::size_t Capacity>
class StampRing
{
public:
  void insert(const std::int64_t stamp_ns, const MessageT & message)
  {
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
  // rclcpp::Time은 sec/nanosec ROS 메시지를 하나의 signed nanosecond join key로 바꾼다.
  return rclcpp::Time(stamp).nanoseconds();
}
}  // namespace

class EventFlowAuditor final : public rclcpp::Node
{
public:
  EventFlowAuditor()
  : Node("event_flow_auditor")
  {
    truth_subscription_ = create_subscription<geometry_msgs::msg::Vector3Stamped>(
      "/sim/event_flow_truth", rclcpp::QoS(10).reliable(),
      [this](const geometry_msgs::msg::Vector3Stamped::ConstSharedPtr message) {
        const auto stamp_ns = stamp_to_nanoseconds(message->header.stamp);
        truths_.insert(stamp_ns, *message);
        try_audit(stamp_ns);
      });
    stats_subscription_ =
      create_subscription<daily_robotics_2026_10_27::msg::EventMotionStats>(
      "/event_camera/motion_stats", rclcpp::QoS(10).reliable(),
      [this](const daily_robotics_2026_10_27::msg::EventMotionStats::ConstSharedPtr message) {
        const auto stamp_ns = stamp_to_nanoseconds(message->header.stamp);
        statistics_.insert(stamp_ns, *message);
        try_audit(stamp_ns);
      });

    // Transient Local durability는 smoke test가 늦게 subscribe해도 마지막 pass/fail을 받게 한다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
  }

private:
  // 같은 stamp의 truth와 estimator telemetry가 모두 있을 때만 한 sample로 인정한다.
  void try_audit(const std::int64_t stamp_ns)
  {
    auto * truth = truths_.find(stamp_ns);
    auto * stats = statistics_.find(stamp_ns);
    if (truth == nullptr || stats == nullptr) {
      return;
    }

    const double error_x = stats->estimated_vx_px_s - truth->vector.x;
    const double error_y = stats->estimated_vy_px_s - truth->vector.y;
    const double speed_error = std::hypot(error_x, error_y);
    squared_error_sum_ += speed_error * speed_error;
    max_error_ = std::max(max_error_, speed_error);
    minimum_valid_events_ = std::min(minimum_valid_events_, stats->valid_events);
    minimum_hypotheses_ = std::min(minimum_hypotheses_, stats->hypotheses_evaluated);
    minimum_peak_ratio_ = std::min(minimum_peak_ratio_, stats->peak_ratio);
    maximum_kernel_us_ = std::max(maximum_kernel_us_, stats->max_kernel_us);
    maximum_queue_drops_ = std::max(maximum_queue_drops_, stats->queue_drops);
    all_contracts_valid_ = all_contracts_valid_ && stats->packet_contract_valid;
    all_peaks_observable_ = all_peaks_observable_ && stats->peak_is_observable;
    all_bounds_respected_ = all_bounds_respected_ && stats->bounds_respected;
    ++audited_samples_;

    truths_.erase(stamp_ns);
    statistics_.erase(stamp_ns);

    if (!result_published_ && audited_samples_ >= 80U) {
      const double rmse = std::sqrt(
        squared_error_sum_ / static_cast<double>(audited_samples_));
      const bool pass = rmse <= 1.0 && max_error_ <= 1.0 &&
        minimum_valid_events_ == 128U && minimum_hypotheses_ == 49U &&
        minimum_peak_ratio_ > 1.001 && maximum_queue_drops_ == 0U &&
        all_contracts_valid_ && all_peaks_observable_ && all_bounds_respected_;

      std_msgs::msg::Bool result;
      result.data = pass;
      pass_publisher_->publish(result);
      result_published_ = true;

      if (pass) {
        RCLCPP_INFO(
          get_logger(),
          "AUDIT PASS samples=%u flow_rmse=%.6f px/s max_error=%.6f px/s "
          "min_events=%u min_hypotheses=%u min_peak_ratio=%.6f max_kernel=%.3f us drops=%u",
          audited_samples_, rmse, max_error_, minimum_valid_events_, minimum_hypotheses_,
          minimum_peak_ratio_, maximum_kernel_us_, maximum_queue_drops_);
      } else {
        RCLCPP_ERROR(
          get_logger(),
          "AUDIT FAIL samples=%u flow_rmse=%.6f max_error=%.6f min_events=%u "
          "min_hypotheses=%u min_peak_ratio=%.6f contract=%s observable=%s bounds=%s drops=%u",
          audited_samples_, rmse, max_error_, minimum_valid_events_, minimum_hypotheses_,
          minimum_peak_ratio_, all_contracts_valid_ ? "true" : "false",
          all_peaks_observable_ ? "true" : "false",
          all_bounds_respected_ ? "true" : "false", maximum_queue_drops_);
      }
    }
  }

  rclcpp::Subscription<geometry_msgs::msg::Vector3Stamped>::SharedPtr truth_subscription_;
  rclcpp::Subscription<daily_robotics_2026_10_27::msg::EventMotionStats>::SharedPtr
    stats_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;

  StampRing<geometry_msgs::msg::Vector3Stamped, kAuditSlots> truths_;
  StampRing<daily_robotics_2026_10_27::msg::EventMotionStats, kAuditSlots> statistics_;
  bool result_published_{false};
  bool all_contracts_valid_{true};
  bool all_peaks_observable_{true};
  bool all_bounds_respected_{true};
  double squared_error_sum_{0.0};
  double max_error_{0.0};
  double minimum_peak_ratio_{std::numeric_limits<double>::infinity()};
  double maximum_kernel_us_{0.0};
  std::uint32_t minimum_valid_events_{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t minimum_hypotheses_{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t maximum_queue_drops_{0U};
  std::uint32_t audited_samples_{0U};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // auditor는 작은 fixed-ring join만 수행하므로 single-threaded spin으로 ownership을 단순화한다.
  rclcpp::spin(std::make_shared<EventFlowAuditor>());
  rclcpp::shutdown();
  return 0;
}
