#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_10_23/msg/estimator_output.hpp"
#include "daily_robotics_2026_10_23/msg/safety_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"

namespace
{
constexpr double kDt = 0.005;
constexpr double kMaximumEffortRate = 40.0;  // [N m/s]
constexpr double kMaximumPositivePower = 2.0;  // [W]
constexpr double kMaximumEnergyBucket = 0.80;  // [J]
constexpr double kEnergyLeakRate = 0.25;  // 쉬는 동안 회복되는 budget [J/s]
constexpr double kVelocityEpsilon = 1.0e-4;
}  // namespace

class EnergyRateSupervisor : public rclcpp::Node
{
public:
  EnergyRateSupervisor()
  : Node("energy_rate_supervisor")
  {
    safe_effort_publisher_ = create_publisher<std_msgs::msg::Float64>(
      "/study/safe_effort", rclcpp::QoS(1).best_effort());
    status_publisher_ = create_publisher<daily_robotics_2026_10_23::msg::SafetyStatus>(
      "/study/safety_status", rclcpp::SensorDataQoS());

    // supervisor는 raw 명령과 state estimate가 하나의 sequence에 묶인 메시지만 받는다.
    // 서로 다른 Topic을 최신값으로 임의 결합할 때 생기는 timestamp mismatch를 피한다.
    estimate_subscription_ =
      create_subscription<daily_robotics_2026_10_23::msg::EstimatorOutput>(
      "/study/estimate", rclcpp::SensorDataQoS(),
      std::bind(&EnergyRateSupervisor::on_estimate, this, std::placeholders::_1));
  }

private:
  // raw torque를 slew-rate, positive mechanical power, leaky energy budget 순서로 투영한다.
  // 계산은 분기 몇 개뿐인 O(1) 경로이며 메모리 할당, lock, I/O를 하지 않는다.
  void on_estimate(const daily_robotics_2026_10_23::msg::EstimatorOutput & estimate)
  {
    const auto cycle_start = std::chrono::steady_clock::now();

    const double maximum_step = kMaximumEffortRate * kDt;
    double safe_effort = std::clamp(
      estimate.raw_effort,
      previous_safe_effort_ - maximum_step,
      previous_safe_effort_ + maximum_step);
    const bool rate_limited = std::abs(safe_effort - estimate.raw_effort) > 1.0e-9;
    if (rate_limited) {
      ++rate_limit_count_;
    }

    // 기계적 출력은 P=tau*q_dot이다. P>0이면 actuator가 robot에 에너지를 주입하고,
    // P<=0이면 제동/회생 영역이므로 이 예제의 positive-power cap 대상이 아니다.
    bool power_limited = false;
    const double velocity = estimate.velocity;
    if (safe_effort * velocity > kMaximumPositivePower &&
      std::abs(velocity) > kVelocityEpsilon)
    {
      safe_effort = kMaximumPositivePower / velocity;
      power_limited = true;
      ++power_limit_count_;
    }

    // leaky bucket E_k=max(0,E_{k-1}-r_leak*dt)+max(0,P)*dt는 최근의
    // 양의 기계 에너지 주입을 기억한다. 이는 물리적 battery SOC가 아니라 안전 envelope다.
    energy_bucket_ = std::max(0.0, energy_bucket_ - kEnergyLeakRate * kDt);
    const double remaining_energy = std::max(0.0, kMaximumEnergyBucket - energy_bucket_);
    const double allowed_energy_power = remaining_energy / kDt;
    bool energy_limited = false;
    if (safe_effort * velocity > allowed_energy_power &&
      std::abs(velocity) > kVelocityEpsilon)
    {
      safe_effort = allowed_energy_power / velocity;
      energy_limited = true;
      ++energy_limit_count_;
    }

    const double positive_power = std::max(0.0, safe_effort * velocity);
    energy_bucket_ = std::min(
      kMaximumEnergyBucket, energy_bucket_ + positive_power * kDt);

    // power/energy 투영은 위험 에너지를 즉시 줄이기 위해 slew 제한보다 우선할 수 있다.
    // 즉, rate limit은 정상 명령의 급변을 완화하고 emergency derating은 빠른 감소를 허용한다.
    previous_safe_effort_ = safe_effort;

    std_msgs::msg::Float64 safe_command;
    safe_command.data = safe_effort;
    safe_effort_publisher_->publish(safe_command);

    const auto cycle_end = std::chrono::steady_clock::now();
    const double cycle_time_us =
      std::chrono::duration<double, std::micro>(cycle_end - cycle_start).count();

    daily_robotics_2026_10_23::msg::SafetyStatus status;
    status.header = estimate.header;
    status.sequence = estimate.sequence;
    status.raw_effort = estimate.raw_effort;
    status.safe_effort = safe_effort;
    status.estimated_velocity = velocity;
    status.positive_power = positive_power;
    status.energy_bucket = energy_bucket_;
    status.rate_limited = rate_limited;
    status.power_limited = power_limited;
    status.energy_limited = energy_limited;
    status.rate_limit_count = rate_limit_count_;
    status.power_limit_count = power_limit_count_;
    status.energy_limit_count = energy_limit_count_;
    status.cycle_time_us = cycle_time_us;
    status_publisher_->publish(status);

    if (estimate.sequence % 200U == 0U) {
      RCLCPP_INFO(
        get_logger(),
        "safety seq=%u raw=%.3f safe=%.3f P+=%.3f E=%.3f limits(r/p/e)=%u/%u/%u",
        estimate.sequence, estimate.raw_effort, safe_effort, positive_power,
        energy_bucket_, rate_limit_count_, power_limit_count_, energy_limit_count_);
    }
  }

  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr safe_effort_publisher_;
  rclcpp::Publisher<daily_robotics_2026_10_23::msg::SafetyStatus>::SharedPtr
    status_publisher_;
  rclcpp::Subscription<daily_robotics_2026_10_23::msg::EstimatorOutput>::SharedPtr
    estimate_subscription_;

  double previous_safe_effort_{0.0};
  double energy_bucket_{0.0};
  std::uint32_t rate_limit_count_{0U};
  std::uint32_t power_limit_count_{0U};
  std::uint32_t energy_limit_count_{0U};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // 안전 상태가 단일 callback에서 순차 갱신되므로 mutex와 priority inversion 경로가 없다.
  rclcpp::spin(std::make_shared<EnergyRateSupervisor>());
  rclcpp::shutdown();
  return 0;
}
