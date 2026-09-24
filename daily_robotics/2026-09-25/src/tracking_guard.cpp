#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

// 서버가 50 Hz에서 발행하는 목표/실제 고정 길이 snapshot 타입이다.
#include "daily_robotics_2026_09_25/msg/tracking_frame.hpp"

/**
 * 서버의 Action Result를 그대로 신뢰하지 않고 추종 frame을 독립 계산해 PASS/FAIL을 내리는 노드다.
 * 표본 수, 유한성, tick 단조성, 목표 연속성, 비영점 시작/종료 속도, 최종 오차를 함께 검사한다.
 */
class TrackingGuard : public rclcpp::Node
{
public:
  using TrackingFrame = daily_robotics_2026_09_25::msg::TrackingFrame;

  TrackingGuard()
  : Node("tracking_guard")
  {
    // audit는 최종 결과이므로 reliable+Transient Local로 늦은 smoke-test 구독자에게도 남긴다.
    audit_publisher_ = create_publisher<std_msgs::msg::String>(
      "/trajectory/audit", rclcpp::QoS(1).reliable().transient_local());

    // 서버와 QoS를 맞춰 terminal TrackingFrame도 durability cache에서 받을 수 있게 한다.
    tracking_subscription_ = create_subscription<TrackingFrame>(
      "/trajectory/tracking_frame",
      rclcpp::QoS(10).reliable().transient_local(),
      std::bind(&TrackingGuard::on_tracking_frame, this, std::placeholders::_1));
  }

private:
  /** 세 원소 중 가장 큰 절댓값을 계산한다. 비영점 속도 여부와 오차 판정에 재사용한다. */
  static double max_abs(const std::array<double, 3> & values)
  {
    double maximum = 0.0;
    for (const double value : values) {
      maximum = std::max(maximum, std::abs(value));
    }
    return maximum;
  }

  /** 고정 길이 추종 표본 하나를 검증하고 terminal 표본에서 한 번만 최종 감사 결과를 발행한다. */
  void on_tracking_frame(const TrackingFrame::SharedPtr message)
  {
    if (audit_published_) {
      return;
    }

    ++sample_count_;
    bool sample_finite = std::isfinite(message->max_position_error_rad);
    double recomputed_error = 0.0;
    for (std::size_t joint = 0; joint < 3; ++joint) {
      sample_finite = sample_finite &&
        std::isfinite(message->desired_position_rad[joint]) &&
        std::isfinite(message->desired_velocity_rad_s[joint]) &&
        std::isfinite(message->desired_acceleration_rad_s2[joint]) &&
        std::isfinite(message->actual_position_rad[joint]) &&
        std::isfinite(message->actual_velocity_rad_s[joint]);
      recomputed_error = std::max(
        recomputed_error,
        std::abs(message->desired_position_rad[joint] - message->actual_position_rad[joint]));
    }
    all_finite_ = all_finite_ && sample_finite;
    max_recomputed_error_ = std::max(max_recomputed_error_, recomputed_error);
    max_report_disagreement_ = std::max(
      max_report_disagreement_,
      std::abs(recomputed_error - message->max_position_error_rad));
    max_lateness_ns_ = std::max(max_lateness_ns_, message->wakeup_lateness_ns);

    if (sample_count_ == 1U) {
      previous_desired_ = message->desired_position_rad;
      nonzero_start_velocity_ = max_abs(message->desired_velocity_rad_s) > 0.04;
    } else {
      // control_ticks는 50 Hz 표본 사이에 약 10씩 증가해야 하며 역행/중복하면 snapshot 계약 위반이다.
      tick_monotonic_ = tick_monotonic_ && message->control_ticks > previous_ticks_;
      double step = 0.0;
      for (std::size_t joint = 0; joint < 3; ++joint) {
        step = std::max(
          step, std::abs(message->desired_position_rad[joint] - previous_desired_[joint]));
      }
      max_desired_step_ = std::max(max_desired_step_, step);
      previous_desired_ = message->desired_position_rad;
    }
    previous_ticks_ = message->control_ticks;

    if (!message->terminal) {
      return;
    }

    const std::array<double, 3> expected_goal{{0.80, -0.50, 0.35}};
    double desired_goal_error = 0.0;
    double actual_goal_error = 0.0;
    for (std::size_t joint = 0; joint < 3; ++joint) {
      desired_goal_error = std::max(
        desired_goal_error,
        std::abs(message->desired_position_rad[joint] - expected_goal[joint]));
      actual_goal_error = std::max(
        actual_goal_error,
        std::abs(message->actual_position_rad[joint] - expected_goal[joint]));
    }
    const bool nonzero_end_velocity = max_abs(message->desired_velocity_rad_s) > 0.04;

    // Action result와 독립된 합격 조건이다. jitter는 환경 의존 관측값이라 기록만 하고 pass gate로 쓰지 않는다.
    const bool passed =
      sample_count_ >= 80U && all_finite_ && tick_monotonic_ &&
      nonzero_start_velocity_ && nonzero_end_velocity &&
      max_recomputed_error_ <= 0.025 && max_report_disagreement_ <= 1.0e-9 &&
      max_desired_step_ <= 0.08 && desired_goal_error <= 1.0e-6 &&
      actual_goal_error <= 0.025;

    std::ostringstream report;
    report.setf(std::ios::fixed);
    report.precision(6);
    report << (passed ? "AUDIT_PASS" : "AUDIT_FAIL")
           << " samples=" << sample_count_
           << " nonzero_start=" << (nonzero_start_velocity_ ? 1 : 0)
           << " nonzero_end=" << (nonzero_end_velocity ? 1 : 0)
           << " max_error_rad=" << max_recomputed_error_
           << " max_step_rad=" << max_desired_step_
           << " terminal_error_rad=" << actual_goal_error
           << " max_lateness_us=" << static_cast<double>(max_lateness_ns_) / 1000.0;

    std_msgs::msg::String audit;
    audit.data = report.str();
    audit_publisher_->publish(audit);
    audit_published_ = true;

    if (passed) {
      RCLCPP_INFO(get_logger(), "%s", audit.data.c_str());
    } else {
      RCLCPP_ERROR(get_logger(), "%s", audit.data.c_str());
    }
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_publisher_;
  rclcpp::Subscription<TrackingFrame>::SharedPtr tracking_subscription_;

  bool audit_published_{false};
  bool all_finite_{true};
  bool tick_monotonic_{true};
  bool nonzero_start_velocity_{false};
  std::size_t sample_count_{0};
  std::uint64_t previous_ticks_{0};
  std::array<double, 3> previous_desired_{};
  double max_recomputed_error_{0.0};
  double max_report_disagreement_{0.0};
  double max_desired_step_{0.0};
  std::int64_t max_lateness_ns_{0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // spin은 TrackingFrame subscription callback을 실행해 독립 감사 상태를 누적한다.
  rclcpp::spin(std::make_shared<TrackingGuard>());
  rclcpp::shutdown();
  return 0;
}
