#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/bool.hpp"

#include "daily_robotics_2026_09_28/msg/control_stats.hpp"

using ControlStats = daily_robotics_2026_09_28::msg::ControlStats;

// 이 노드는 토크 생성에 참여하지 않는 독립 관찰자다.
// 원본 JointState와 저주기 ControlStats를 timestamp로 맞춰 FK/오차/수치/시간 계약을 재검산한다.
class ControlAuditor : public rclcpp::Node
{
public:
  ControlAuditor()
  : Node("control_auditor")
  {
    const auto state_qos = rclcpp::SensorDataQoS().keep_last(1);
    state_subscription_ = create_subscription<sensor_msgs::msg::JointState>(
      "/arm/joint_states", state_qos,
      std::bind(&ControlAuditor::on_joint_state, this, std::placeholders::_1));

    stats_subscription_ = create_subscription<ControlStats>(
      "/arm/control_stats", rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&ControlAuditor::on_stats, this, std::placeholders::_1));

    // transient_local은 PASS가 한 번만 발행되어도 뒤늦게 붙은 smoke test가 마지막 값을 받게 한다.
    const auto result_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>("/study/audit_pass", result_qos);

    RCLCPP_INFO(get_logger(), "Independent controller auditor ready");
  }

private:
  struct StateSample
  {
    std::int64_t stamp_nanoseconds{0};
    std::array<double, 3> q{};
    bool valid{false};
  };

  // name 기반으로 3개 관절을 추출한다. controller와 별도 구현해 같은 인덱스 가정을 공유하지 않는다.
  static bool extract_positions(
    const sensor_msgs::msg::JointState & message,
    std::array<double, 3> & q)
  {
    constexpr std::array<const char *, 3> kExpectedNames{
      "joint_1", "joint_2", "joint_3"};
    if (message.position.size() != message.name.size()) {
      return false;
    }
    for (std::size_t expected = 0; expected < 3; ++expected) {
      bool found = false;
      for (std::size_t index = 0; index < message.name.size(); ++index) {
        if (message.name[index] == kExpectedNames[expected]) {
          q[expected] = message.position[index];
          found = std::isfinite(q[expected]);
          break;
        }
      }
      if (!found) {
        return false;
      }
    }
    return true;
  }

  // 감사기가 자체 구현한 FK다. controller의 arm_model.hpp 함수를 호출하지 않아 공통 코드 오류를 숨기지 않는다.
  static std::array<double, 2> independent_forward_kinematics(
    const std::array<double, 3> & q)
  {
    constexpr std::array<double, 3> kLength{0.60, 0.45, 0.30};
    const std::array<double, 3> theta{q[0], q[0] + q[1], q[0] + q[1] + q[2]};
    std::array<double, 2> position{};
    for (std::size_t link = 0; link < 3; ++link) {
      position[0] += kLength[link] * std::cos(theta[link]);
      position[1] += kLength[link] * std::sin(theta[link]);
    }
    return position;
  }

  // 500 Hz 상태를 고정 64칸 ring에 보관한다. vector/deque 증가 없이 최근 128 ms를 덮어쓴다.
  void on_joint_state(const sensor_msgs::msg::JointState::SharedPtr message)
  {
    std::array<double, 3> q{};
    if (!extract_positions(*message, q)) {
      ++invalid_state_messages_;
      return;
    }
    StateSample & slot = state_ring_[state_write_index_];
    slot.stamp_nanoseconds = rclcpp::Time(message->header.stamp).nanoseconds();
    slot.q = q;
    slot.valid = true;
    state_write_index_ = (state_write_index_ + 1U) % state_ring_.size();
    ++state_samples_;
  }

  bool find_matching_state(
    const std::int64_t stamp_nanoseconds,
    std::array<double, 3> & q) const
  {
    for (const StateSample & sample : state_ring_) {
      if (sample.valid && sample.stamp_nanoseconds == stamp_nanoseconds) {
        q = sample.q;
        return true;
      }
    }
    return false;
  }

  // ControlStats 한 건을 원본 q와 대조하고 연속 30건이 모두 건전할 때만 PASS를 latch한다.
  void on_stats(const ControlStats::SharedPtr message)
  {
    if (passed_) {
      return;
    }

    const std::int64_t stamp_nanoseconds = rclcpp::Time(message->header.stamp).nanoseconds();
    std::array<double, 3> q{};
    if (!find_matching_state(stamp_nanoseconds, q)) {
      ++unmatched_stats_;
      return;
    }

    const std::array<double, 2> independent_position = independent_forward_kinematics(q);
    const double fk_difference = std::hypot(
      independent_position[0] - message->actual_xy_m[0],
      independent_position[1] - message->actual_xy_m[1]);
    const double recomputed_error = std::hypot(
      message->desired_xy_m[0] - message->actual_xy_m[0],
      message->desired_xy_m[1] - message->actual_xy_m[1]);
    const double error_claim_difference = std::abs(recomputed_error - message->task_error_m);

    const bool sequence_ok = last_sequence_ == 0U || message->sequence > last_sequence_;
    last_sequence_ = message->sequence;
    max_callback_us_ = std::fmax(max_callback_us_, message->callback_us);

    // 이 threshold들은 교육용 acceptance contract다. 평균 실행시간이 아닌 샘플 관측값이며 WCET 증명이 아니다.
    const bool finite = std::isfinite(message->task_error_m) &&
      std::isfinite(message->nullspace_leakage) &&
      std::isfinite(message->task_inertia_det) &&
      std::isfinite(message->torque_norm_nm) &&
      std::isfinite(message->callback_us);
    const bool math_ok = fk_difference < 1.0e-9 && error_claim_difference < 1.0e-9;
    const bool control_ok = message->task_error_m < 0.045 &&
      message->nullspace_leakage < 0.050 &&
      message->task_inertia_det > 1.0e-6 &&
      message->torque_norm_nm <= 104.0;
    const bool timing_ok = message->callback_us > 0.0 && message->callback_us < 5000.0;
    const bool good = finite && math_ok && control_ok && timing_ok && sequence_ok;

    if (good && state_samples_ >= 500U) {
      ++good_streak_;
    } else {
      good_streak_ = 0U;
    }

    if (good_streak_ >= 30U) {
      passed_ = true;
      std_msgs::msg::Bool result;
      result.data = true;
      pass_publisher_->publish(result);
      RCLCPP_INFO(
        get_logger(),
        "AUDIT_PASS states=%llu stats_sequence=%llu error=%.6f leakage=%.9f "
        "det=%.6f guard=%s torque_norm=%.3f max_callback_us=%.3f unmatched=%llu",
        static_cast<unsigned long long>(state_samples_),
        static_cast<unsigned long long>(message->sequence),
        message->task_error_m,
        message->nullspace_leakage,
        message->task_inertia_det,
        message->singularity_guard_active ? "active" : "inactive",
        message->torque_norm_nm,
        max_callback_us_,
        static_cast<unsigned long long>(unmatched_stats_));
    }
  }

  std::array<StateSample, 64> state_ring_{};
  std::size_t state_write_index_{0};
  std::uint64_t state_samples_{0};
  std::uint64_t invalid_state_messages_{0};
  std::uint64_t unmatched_stats_{0};
  std::uint64_t last_sequence_{0};
  std::uint32_t good_streak_{0};
  double max_callback_us_{0.0};
  bool passed_{false};

  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr state_subscription_;
  rclcpp::Subscription<ControlStats>::SharedPtr stats_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ControlAuditor>());
  rclcpp::shutdown();
  return 0;
}
