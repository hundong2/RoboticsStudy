#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_10_23/msg/actuator_sample.hpp"
#include "daily_robotics_2026_10_23/msg/estimator_output.hpp"
#include "daily_robotics_2026_10_23/msg/safety_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
constexpr std::size_t kAuditCapacity = 512U;
constexpr std::uint64_t kRequiredMatches = 1200U;
constexpr double kMaximumPositivePower = 2.0;
constexpr double kMaximumEnergyBucket = 0.80;
constexpr double kMaximumNormalEffortStep = 40.0 * 0.005;

struct MatchSlot
{
  std::uint32_t sequence{0U};
  bool sample_ready{false};
  bool estimate_ready{false};
  bool counted{false};
  double truth_position{0.0};
  double learned_position{0.0};
  double ood_score{0.0};
  double estimated_position{0.0};
  bool learned_accepted{false};
};
}  // namespace

class PipelineAuditor : public rclcpp::Node
{
public:
  PipelineAuditor()
  : Node("pipeline_auditor")
  {
    // transient_local은 auditor가 PASS를 한 번만 publish해도 늦게 붙은 smoke-test가 받게 한다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(1).reliable().transient_local());

    sample_subscription_ =
      create_subscription<daily_robotics_2026_10_23::msg::ActuatorSample>(
      "/study/actuator_sample", rclcpp::SensorDataQoS(),
      std::bind(&PipelineAuditor::on_sample, this, std::placeholders::_1));
    estimate_subscription_ =
      create_subscription<daily_robotics_2026_10_23::msg::EstimatorOutput>(
      "/study/estimate", rclcpp::SensorDataQoS(),
      std::bind(&PipelineAuditor::on_estimate, this, std::placeholders::_1));
    safety_subscription_ =
      create_subscription<daily_robotics_2026_10_23::msg::SafetyStatus>(
      "/study/safety_status", rclcpp::SensorDataQoS(),
      std::bind(&PipelineAuditor::on_safety, this, std::placeholders::_1));
  }

private:
  // sequence modulo 고정 ring으로 서로 다른 callback에서 온 truth와 estimate를 정확히 맞춘다.
  // 최신값끼리 비교하면 DDS scheduling 지연 때문에 다른 시각의 상태를 비교할 수 있다.
  MatchSlot & prepare_slot(const std::uint32_t sequence)
  {
    auto & slot = slots_[sequence % kAuditCapacity];
    if (slot.sequence != sequence) {
      slot = MatchSlot{};
      slot.sequence = sequence;
    }
    return slot;
  }

  void on_sample(const daily_robotics_2026_10_23::msg::ActuatorSample & sample)
  {
    auto & slot = prepare_slot(sample.sequence);
    slot.sample_ready = true;
    slot.truth_position = sample.truth_position;
    slot.learned_position = sample.learned_position;
    slot.ood_score = sample.ood_score;
    count_if_complete(slot);
  }

  void on_estimate(const daily_robotics_2026_10_23::msg::EstimatorOutput & estimate)
  {
    auto & slot = prepare_slot(estimate.sequence);
    slot.estimate_ready = true;
    slot.estimated_position = estimate.position;
    slot.learned_accepted = estimate.learned_measurement_accepted;
    final_calibration_scale_ = estimate.calibration_scale;
    final_ood_reject_count_ = estimate.ood_reject_count;
    final_nis_reject_count_ = estimate.nis_reject_count;
    maximum_estimator_cycle_us_ = std::max(
      maximum_estimator_cycle_us_, estimate.cycle_time_us);
    count_if_complete(slot);
  }

  void count_if_complete(MatchSlot & slot)
  {
    if (!slot.sample_ready || !slot.estimate_ready || slot.counted) {
      return;
    }
    slot.counted = true;
    ++matched_count_;

    const double estimate_error = slot.estimated_position - slot.truth_position;
    estimate_squared_error_sum_ += estimate_error * estimate_error;

    if (slot.ood_score <= 0.80) {
      const double learned_error = slot.learned_position - slot.truth_position;
      learned_squared_error_sum_ += learned_error * learned_error;
      ++in_domain_count_;
    } else if (slot.learned_accepted) {
      ++accepted_ood_count_;
    }

    evaluate_once_ready();
  }

  // estimator와 별개로 safe command 자체의 수학적 불변식을 검사한다.
  // emergency power/energy 감소는 slew comfort limit보다 우선하므로 그 tick은 rate 검사에서 뺀다.
  void on_safety(const daily_robotics_2026_10_23::msg::SafetyStatus & status)
  {
    ++safety_count_;
    if (status.positive_power > kMaximumPositivePower + 1.0e-6 ||
      status.energy_bucket > kMaximumEnergyBucket + 1.0e-6 ||
      !std::isfinite(status.safe_effort))
    {
      ++safety_envelope_violation_count_;
    }

    if (have_previous_safety_ && status.sequence == previous_safety_sequence_ + 1U &&
      !status.power_limited && !status.energy_limited &&
      std::abs(status.safe_effort - previous_safe_effort_) >
      kMaximumNormalEffortStep + 1.0e-6)
    {
      ++normal_rate_violation_count_;
    }
    have_previous_safety_ = true;
    previous_safety_sequence_ = status.sequence;
    previous_safe_effort_ = status.safe_effort;

    final_rate_limit_count_ = status.rate_limit_count;
    final_power_limit_count_ = status.power_limit_count;
    final_energy_limit_count_ = status.energy_limit_count;
    maximum_supervisor_cycle_us_ = std::max(
      maximum_supervisor_cycle_us_, status.cycle_time_us);
    evaluate_once_ready();
  }

  void evaluate_once_ready()
  {
    if (result_published_ || matched_count_ < kRequiredMatches ||
      safety_count_ < kRequiredMatches || in_domain_count_ == 0U)
    {
      return;
    }

    const double estimate_rmse = std::sqrt(
      estimate_squared_error_sum_ / static_cast<double>(matched_count_));
    const double learned_rmse = std::sqrt(
      learned_squared_error_sum_ / static_cast<double>(in_domain_count_));

    const bool pass =
      estimate_rmse < learned_rmse &&
      accepted_ood_count_ == 0U &&
      final_ood_reject_count_ >= 100U &&
      final_calibration_scale_ > 2.0 &&
      final_calibration_scale_ <= 25.0 &&
      final_rate_limit_count_ > 0U &&
      final_power_limit_count_ > 0U &&
      final_energy_limit_count_ > 0U &&
      safety_envelope_violation_count_ == 0U &&
      normal_rate_violation_count_ == 0U;

    if (pass) {
      RCLCPP_INFO(
        get_logger(),
        "AUDIT PASS matched=%llu estimate_rmse=%.6f learned_in_domain_rmse=%.6f "
        "scale=%.3f reject(ood/nis)=%u/%u accepted_ood=%llu limits(r/p/e)=%u/%u/%u "
        "violations(envelope/rate)=%llu/%llu max_cycle_us(filter/supervisor)=%.3f/%.3f",
        static_cast<unsigned long long>(matched_count_), estimate_rmse, learned_rmse,
        final_calibration_scale_, final_ood_reject_count_, final_nis_reject_count_,
        static_cast<unsigned long long>(accepted_ood_count_), final_rate_limit_count_,
        final_power_limit_count_, final_energy_limit_count_,
        static_cast<unsigned long long>(safety_envelope_violation_count_),
        static_cast<unsigned long long>(normal_rate_violation_count_),
        maximum_estimator_cycle_us_, maximum_supervisor_cycle_us_);
    } else {
      RCLCPP_ERROR(
        get_logger(),
        "AUDIT FAIL matched=%llu estimate_rmse=%.6f learned_rmse=%.6f scale=%.3f "
        "reject_ood=%u accepted_ood=%llu limits=%u/%u/%u violations=%llu/%llu",
        static_cast<unsigned long long>(matched_count_), estimate_rmse, learned_rmse,
        final_calibration_scale_, final_ood_reject_count_,
        static_cast<unsigned long long>(accepted_ood_count_), final_rate_limit_count_,
        final_power_limit_count_, final_energy_limit_count_,
        static_cast<unsigned long long>(safety_envelope_violation_count_),
        static_cast<unsigned long long>(normal_rate_violation_count_));
    }

    std_msgs::msg::Bool result;
    result.data = pass;
    pass_publisher_->publish(result);
    result_published_ = true;
  }

  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;
  rclcpp::Subscription<daily_robotics_2026_10_23::msg::ActuatorSample>::SharedPtr
    sample_subscription_;
  rclcpp::Subscription<daily_robotics_2026_10_23::msg::EstimatorOutput>::SharedPtr
    estimate_subscription_;
  rclcpp::Subscription<daily_robotics_2026_10_23::msg::SafetyStatus>::SharedPtr
    safety_subscription_;

  std::array<MatchSlot, kAuditCapacity> slots_{};
  bool result_published_{false};
  bool have_previous_safety_{false};
  std::uint32_t previous_safety_sequence_{0U};
  double previous_safe_effort_{0.0};

  std::uint64_t matched_count_{0U};
  std::uint64_t in_domain_count_{0U};
  std::uint64_t safety_count_{0U};
  std::uint64_t accepted_ood_count_{0U};
  std::uint64_t safety_envelope_violation_count_{0U};
  std::uint64_t normal_rate_violation_count_{0U};
  double estimate_squared_error_sum_{0.0};
  double learned_squared_error_sum_{0.0};
  double final_calibration_scale_{1.0};
  std::uint32_t final_ood_reject_count_{0U};
  std::uint32_t final_nis_reject_count_{0U};
  std::uint32_t final_rate_limit_count_{0U};
  std::uint32_t final_power_limit_count_{0U};
  std::uint32_t final_energy_limit_count_{0U};
  double maximum_estimator_cycle_us_{0.0};
  double maximum_supervisor_cycle_us_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // audit ring도 한 thread에서만 만지므로 callback 간 lock과 race가 없다.
  rclcpp::spin(std::make_shared<PipelineAuditor>());
  rclcpp::shutdown();
  return 0;
}
