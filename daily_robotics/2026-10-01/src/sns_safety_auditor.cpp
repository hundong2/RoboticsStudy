#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

#include "daily_robotics_2026_10_01/msg/joint_velocity_command.hpp"
#include "daily_robotics_2026_10_01/msg/sns_status.hpp"

using JointVelocityCommand = daily_robotics_2026_10_01::msg::JointVelocityCommand;
using SnsStatus = daily_robotics_2026_10_01::msg::SnsStatus;

namespace
{
using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;
using Mat23 = std::array<Vec3, 2>;

constexpr Vec3 kExpectedLimitRadS{0.35, 0.25, 0.45};
constexpr std::size_t kRingCapacity = 64;

struct CommandSlot
{
  std::uint64_t sequence{0};
  Vec3 velocity{};
  bool valid{false};
};

struct StatusSlot
{
  std::uint64_t sequence{0};
  Vec3 joint_position{};
  Vec2 desired{};
  Vec2 reported_achieved{};
  double task_scale{0.0};
  double reported_residual{0.0};
  double condition_number{0.0};
  double callback_us{0.0};
  std::uint8_t saturated_mask{0};
  bool singularity_guard{false};
  bool numerical_fallback{false};
  bool valid{false};
};

// 제어기 함수를 호출하지 않고 감사 노드가 같은 3R 기구학을 독립 계산한다.
// 독립 경로를 두면 제어기 내부의 잘못된 achieved 값과 실제 명령을 서로 비교할 수 있다.
Mat23 independent_jacobian(const Vec3 & q)
{
  constexpr double kL1 = 0.70;
  constexpr double kL2 = 0.55;
  constexpr double kL3 = 0.35;
  const double q12 = q[0] + q[1];
  const double q123 = q12 + q[2];
  return Mat23{
    Vec3{
      -kL1 * std::sin(q[0]) - kL2 * std::sin(q12) - kL3 * std::sin(q123),
      -kL2 * std::sin(q12) - kL3 * std::sin(q123),
      -kL3 * std::sin(q123)},
    Vec3{
      kL1 * std::cos(q[0]) + kL2 * std::cos(q12) + kL3 * std::cos(q123),
      kL2 * std::cos(q12) + kL3 * std::cos(q123),
      kL3 * std::cos(q123)}};
}

Vec2 independent_achieved_velocity(const Mat23 & jacobian, const Vec3 & velocity)
{
  Vec2 achieved{};
  for (std::size_t axis = 0; axis < 2; ++axis) {
    for (std::size_t joint = 0; joint < 3; ++joint) {
      achieved[axis] += jacobian[axis][joint] * velocity[joint];
    }
  }
  return achieved;
}
}  // namespace

// 이 노드는 제어 명령을 actuator에 보내기 전의 독립 safety monitor를 흉내 낸다.
// 같은 sequence의 명령/진단을 짝지어 속도 한계, J*q_dot, task scale, 잔차를 다시 검사한다.
class SnsSafetyAuditor : public rclcpp::Node
{
public:
  SnsSafetyAuditor()
  : Node("sns_safety_auditor")
  {
    command_subscription_ = create_subscription<JointVelocityCommand>(
      "/joint_velocity_command",
      rclcpp::QoS(rclcpp::KeepLast(32)).best_effort(),
      std::bind(&SnsSafetyAuditor::on_command, this, std::placeholders::_1));
    status_subscription_ = create_subscription<SnsStatus>(
      "/sns/status",
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
      std::bind(&SnsSafetyAuditor::on_status, this, std::placeholders::_1));

    // transient_local은 감사 완료 뒤 smoke test가 구독해도 마지막 PASS 값을 즉시 재전송한다.
    audit_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/audit_pass",
      rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());

    RCLCPP_INFO(get_logger(), "Independent SNS auditor ready: 64-sequence fixed rings");
  }

private:
  // Topic 간 도착 순서는 보장되지 않으므로 sequence%64 위치에 명령을 저장하고 짝이 오면 감사한다.
  void on_command(const JointVelocityCommand::SharedPtr message)
  {
    const std::size_t slot_index = static_cast<std::size_t>(message->sequence % kRingCapacity);
    command_slots_[slot_index].sequence = message->sequence;
    command_slots_[slot_index].velocity = message->velocity_rad_s;
    command_slots_[slot_index].valid = true;
    try_audit(message->sequence);
  }

  void on_status(const SnsStatus::SharedPtr message)
  {
    const std::size_t slot_index = static_cast<std::size_t>(message->sequence % kRingCapacity);
    StatusSlot & slot = status_slots_[slot_index];
    slot.sequence = message->sequence;
    slot.joint_position = message->joint_position_rad;
    slot.desired = message->desired_xy_m_s;
    slot.reported_achieved = message->achieved_xy_m_s;
    slot.task_scale = message->task_scale;
    slot.reported_residual = message->scaled_residual_m_s;
    slot.condition_number = message->condition_number;
    slot.callback_us = message->callback_us;
    slot.saturated_mask = message->saturated_mask;
    slot.singularity_guard = message->singularity_guard_active;
    slot.numerical_fallback = message->numerical_fallback_active;
    slot.valid = true;
    try_audit(message->sequence);
  }

  // 동일 sequence의 두 Topic이 모두 도착했을 때만 한 번 검사한다.
  void try_audit(const std::uint64_t sequence)
  {
    const std::size_t slot_index = static_cast<std::size_t>(sequence % kRingCapacity);
    const CommandSlot & command = command_slots_[slot_index];
    const StatusSlot & status = status_slots_[slot_index];
    if (!command.valid || !status.valid || command.sequence != sequence ||
      status.sequence != sequence || audited_sequences_[slot_index] == sequence)
    {
      return;
    }
    audited_sequences_[slot_index] = sequence;

    // 시작 직후 state/target이 오기 전의 명시적 0 폴백은 정상 준비 단계라 통계에서 제외한다.
    if (status.numerical_fallback) {
      ++fallback_samples_;
      return;
    }

    bool sample_ok = true;
    for (std::size_t joint = 0; joint < 3; ++joint) {
      sample_ok = sample_ok && std::isfinite(command.velocity[joint]);
      sample_ok = sample_ok &&
        std::abs(command.velocity[joint]) <= kExpectedLimitRadS[joint] + 1.0e-9;
    }
    sample_ok = sample_ok && std::isfinite(status.task_scale) &&
      status.task_scale >= -1.0e-12 && status.task_scale <= 1.0 + 1.0e-12;

    const Mat23 jacobian = independent_jacobian(status.joint_position);
    const Vec2 independently_achieved = independent_achieved_velocity(jacobian, command.velocity);
    const double report_error = std::hypot(
      independently_achieved[0] - status.reported_achieved[0],
      independently_achieved[1] - status.reported_achieved[1]);
    const double scaled_residual = std::hypot(
      independently_achieved[0] - status.task_scale * status.desired[0],
      independently_achieved[1] - status.task_scale * status.desired[1]);
    const double residual_report_error = std::abs(scaled_residual - status.reported_residual);

    // DLS guard는 특이점에서 exact inverse 대신 bounded 명령을 택하므로 작은 task residual을 허용한다.
    sample_ok = sample_ok && report_error < 1.0e-8;
    sample_ok = sample_ok && residual_report_error < 1.0e-8;
    sample_ok = sample_ok && scaled_residual < 0.02;

    ++audited_samples_;
    if (!sample_ok) {
      ++failed_samples_;
      if (failed_samples_ <= 3U) {
        RCLCPP_ERROR(
          get_logger(),
          "AUDIT_FAIL seq=%llu report_err=%.3e residual=%.5f scale=%.4f mask=%u",
          static_cast<unsigned long long>(sequence), report_error, scaled_residual,
          status.task_scale, static_cast<unsigned int>(status.saturated_mask));
      }
    }

    min_scale_ = std::min(min_scale_, status.task_scale);
    max_condition_ = std::max(max_condition_, status.condition_number);
    max_residual_ = std::max(max_residual_, scaled_residual);
    max_callback_us_ = std::max(max_callback_us_, status.callback_us);
    seen_task_scaling_ = seen_task_scaling_ || status.task_scale < 0.98;
    seen_saturation_ = seen_saturation_ || status.saturated_mask != 0U;
    seen_singularity_guard_ = seen_singularity_guard_ || status.singularity_guard;

    const bool enough_evidence = audited_samples_ >= 80U && seen_task_scaling_ &&
      seen_saturation_ && seen_singularity_guard_;
    if (!published_pass_ && enough_evidence && failed_samples_ == 0U) {
      std_msgs::msg::Bool pass_message;
      pass_message.data = true;
      audit_publisher_->publish(pass_message);
      published_pass_ = true;
      RCLCPP_INFO(
        get_logger(),
        "AUDIT_PASS samples=%llu fallback=%llu min_scale=%.4f max_cond=%.2f "
        "max_residual=%.5f max_kernel_us=%.2f",
        static_cast<unsigned long long>(audited_samples_),
        static_cast<unsigned long long>(fallback_samples_), min_scale_, max_condition_,
        max_residual_, max_callback_us_);
    }
  }

  std::array<CommandSlot, kRingCapacity> command_slots_{};
  std::array<StatusSlot, kRingCapacity> status_slots_{};
  std::array<std::uint64_t, kRingCapacity> audited_sequences_{};
  std::uint64_t audited_samples_{0};
  std::uint64_t failed_samples_{0};
  std::uint64_t fallback_samples_{0};
  double min_scale_{std::numeric_limits<double>::infinity()};
  double max_condition_{0.0};
  double max_residual_{0.0};
  double max_callback_us_{0.0};
  bool seen_task_scaling_{false};
  bool seen_saturation_{false};
  bool seen_singularity_guard_{false};
  bool published_pass_{false};

  rclcpp::Subscription<JointVelocityCommand>::SharedPtr command_subscription_;
  rclcpp::Subscription<SnsStatus>::SharedPtr status_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr audit_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SnsSafetyAuditor>());
  rclcpp::shutdown();
  return 0;
}
