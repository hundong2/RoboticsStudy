#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

#include "daily_robotics_2026_09_30/msg/contact_estimate.hpp"
#include "daily_robotics_2026_09_30/msg/contact_truth.hpp"
#include "daily_robotics_2026_09_30/msg/safety_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

// ContactSafetyAuditor의 역할:
// estimator/supervisor의 자기 보고와 별개로 ground truth 전이, trip latency, force clamp, 복귀를 검증한다.
// 제품에서는 ground truth 대신 HIL 계측기나 독립 센서가 이 역할을 맡는다.
class ContactSafetyAuditor : public rclcpp::Node
{
public:
  ContactSafetyAuditor()
  : Node("contact_safety_auditor")
  {
    truth_subscription_ = create_subscription<daily_robotics_2026_09_30::msg::ContactTruth>(
      "/sim/contact_truth", rclcpp::QoS(10).reliable(),
      [this](const daily_robotics_2026_09_30::msg::ContactTruth::ConstSharedPtr message) {
        on_truth(message);
      });
    estimate_subscription_ =
      create_subscription<daily_robotics_2026_09_30::msg::ContactEstimate>(
      "/contact/estimate", rclcpp::QoS(10).reliable(),
      [this](const daily_robotics_2026_09_30::msg::ContactEstimate::ConstSharedPtr message) {
        on_estimate(message);
      });
    status_subscription_ = create_subscription<daily_robotics_2026_09_30::msg::SafetyStatus>(
      "/safety/status", rclcpp::QoS(10).reliable(),
      [this](const daily_robotics_2026_09_30::msg::SafetyStatus::ConstSharedPtr message) {
        on_status(message);
      });

    // transient_local은 감사기가 PASS를 한 번만 내도 늦게 붙은 smoke client가 값을 받게 한다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/audit_pass", rclcpp::QoS(1).reliable().transient_local());
  }

private:
  void on_truth(const daily_robotics_2026_09_30::msg::ContactTruth::ConstSharedPtr & truth)
  {
    if (truth->mode == 0U) {
      saw_air_ = true;
    } else if (truth->mode == 1U) {
      saw_stable_ = true;
    } else if (truth->mode == 2U) {
      saw_truth_slip_ = true;
      if (first_slip_stamp_ns_ < 0) {
        first_slip_stamp_ns_ = rclcpp::Time(truth->header.stamp).nanoseconds();
      }
    }
  }

  void on_estimate(
    const daily_robotics_2026_09_30::msg::ContactEstimate::ConstSharedPtr & estimate)
  {
    latest_accepted_samples_ = estimate->accepted_samples;
    maximum_callback_us_ = std::max(maximum_callback_us_, estimate->callback_time_us);
    maximum_pair_skew_ms_ = std::max(maximum_pair_skew_ms_, estimate->pair_skew_ms);
    if (estimate->mode == 2U) {
      saw_estimated_slip_ = true;
    }
    evaluate();
  }

  void on_status(const daily_robotics_2026_09_30::msg::SafetyStatus::ConstSharedPtr & status)
  {
    maximum_age_ms_ = std::max(maximum_age_ms_, status->estimate_age_ms);
    // supervisor가 보고한 보수 한계를 단 1e-6 N이라도 넘으면 clamp 구현 오류로 센다.
    if (std::abs(status->allowed_force_n) > status->conservative_limit_n + 1.0e-6) {
      ++limit_violations_;
    }
    if (status->state == 2U) {
      saw_fallback_ = true;
      if (first_fallback_stamp_ns_ < 0) {
        first_fallback_stamp_ns_ = rclcpp::Time(status->header.stamp).nanoseconds();
      }
      // fallback 상태에서 접선 출력은 반드시 정확히 0이어야 한다.
      if (std::abs(status->allowed_force_n) > 1.0e-9) {
        ++fallback_nonzero_violations_;
      }
    }
    if (saw_fallback_ && status->state == 1U && status->trip_count > 0U) {
      saw_recovery_ = true;
    }
    latest_trip_count_ = status->trip_count;
    evaluate();
  }

  void evaluate()
  {
    if (published_ || latest_accepted_samples_ < 4500U) {
      return;
    }
    double trip_latency_ms = std::numeric_limits<double>::infinity();
    if (first_slip_stamp_ns_ >= 0 && first_fallback_stamp_ns_ >= first_slip_stamp_ns_) {
      trip_latency_ms =
        static_cast<double>(first_fallback_stamp_ns_ - first_slip_stamp_ns_) * 1.0e-6;
    }

    if (!diagnostics_printed_) {
      RCLCPP_INFO(
        get_logger(),
        "AUDIT_CHECK air=%d stable=%d truth_slip=%d estimated_slip=%d fallback=%d recovery=%d "
        "samples=%u trips=%u latency_ms=%.3f max_cb_us=%.3f max_skew_ms=%.3f max_age_ms=%.3f",
        saw_air_, saw_stable_, saw_truth_slip_, saw_estimated_slip_, saw_fallback_, saw_recovery_,
        latest_accepted_samples_, latest_trip_count_, trip_latency_ms, maximum_callback_us_,
        maximum_pair_skew_ms_, maximum_age_ms_);
      diagnostics_printed_ = true;
    }

    if (!saw_recovery_) {
      return;
    }

    const bool pass =
      saw_air_ && saw_stable_ && saw_truth_slip_ && saw_estimated_slip_ && saw_fallback_ &&
      saw_recovery_ && latest_trip_count_ >= 1U && trip_latency_ms <= 30.0 &&
      limit_violations_ == 0U && fallback_nonzero_violations_ == 0U &&
      maximum_pair_skew_ms_ <= 3.0 && maximum_callback_us_ < 2000.0 &&
      maximum_age_ms_ < 20.0;
    if (!pass) {
      return;
    }

    std_msgs::msg::Bool result;
    result.data = true;
    pass_publisher_->publish(result);
    published_ = true;
    RCLCPP_INFO(
      get_logger(),
      "AUDIT_PASS samples=%u trips=%u trip_latency_ms=%.3f max_callback_us=%.3f "
      "max_pair_skew_ms=%.3f max_age_ms=%.3f limit_violations=%u",
      latest_accepted_samples_, latest_trip_count_, trip_latency_ms, maximum_callback_us_,
      maximum_pair_skew_ms_, maximum_age_ms_, limit_violations_);
  }

  bool saw_air_{false};
  bool saw_stable_{false};
  bool saw_truth_slip_{false};
  bool saw_estimated_slip_{false};
  bool saw_fallback_{false};
  bool saw_recovery_{false};
  bool published_{false};
  bool diagnostics_printed_{false};
  std::int64_t first_slip_stamp_ns_{-1};
  std::int64_t first_fallback_stamp_ns_{-1};
  std::uint32_t latest_accepted_samples_{0U};
  std::uint32_t latest_trip_count_{0U};
  std::uint32_t limit_violations_{0U};
  std::uint32_t fallback_nonzero_violations_{0U};
  double maximum_callback_us_{0.0};
  double maximum_pair_skew_ms_{0.0};
  double maximum_age_ms_{0.0};
  rclcpp::Subscription<daily_robotics_2026_09_30::msg::ContactTruth>::SharedPtr
    truth_subscription_;
  rclcpp::Subscription<daily_robotics_2026_09_30::msg::ContactEstimate>::SharedPtr
    estimate_subscription_;
  rclcpp::Subscription<daily_robotics_2026_09_30::msg::SafetyStatus>::SharedPtr
    status_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ContactSafetyAuditor>());
  rclcpp::shutdown();
  return 0;
}
