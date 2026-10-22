#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_10_23/msg/actuator_sample.hpp"
#include "daily_robotics_2026_10_23/msg/estimator_output.hpp"
#include "rclcpp/rclcpp.hpp"

namespace
{
constexpr double kDt = 0.005;
constexpr double kEncoderVariance = 2.5e-3;  // sigma=5 cm인 저정밀 encoder 모델
constexpr double kOodThreshold = 0.80;
constexpr double kNisThreshold = 9.0;        // 1자유도 chi-square의 보수적 약 3-sigma gate
constexpr double kScaleLearningRate = 0.005;
}  // namespace

class AdaptiveCovarianceFilter : public rclcpp::Node
{
public:
  AdaptiveCovarianceFilter()
  : Node("adaptive_covariance_filter")
  {
    output_publisher_ =
      create_publisher<daily_robotics_2026_10_23::msg::EstimatorOutput>(
      "/study/estimate", rclcpp::SensorDataQoS());

    // 입력과 출력 모두 SensorDataQoS를 사용해야 DDS QoS 호환성이 맞는다.
    // 이 callback 안에서 filter 상태를 완전히 갱신하고 publish하므로 중간 상태가 노출되지 않는다.
    sample_subscription_ =
      create_subscription<daily_robotics_2026_10_23::msg::ActuatorSample>(
      "/study/actuator_sample", rclcpp::SensorDataQoS(),
      std::bind(&AdaptiveCovarianceFilter::on_sample, this, std::placeholders::_1));
  }

private:
  // H=[1,0]인 scalar 위치 측정으로 [position, velocity] 상태를 보정한다.
  // Joseph form P=(I-KH)P(I-KH)^T+KRK^T를 그대로 전개하여 양의 준정부호성과
  // 대칭성을 단순 P=(I-KH)P보다 수치적으로 더 잘 보존한다.
  void update_position(const double measurement, const double variance)
  {
    const double innovation = measurement - position_;
    const double innovation_variance = p00_ + variance;
    const double k0 = p00_ / innovation_variance;
    const double k1 = p10_ / innovation_variance;

    position_ += k0 * innovation;
    velocity_ += k1 * innovation;

    const double a00 = 1.0 - k0;
    const double a10 = -k1;
    const double b00 = a00 * p00_;
    const double b01 = a00 * p01_;
    const double b10 = a10 * p00_ + p10_;
    const double b11 = a10 * p01_ + p11_;

    const double new_p00 = b00 * a00 + k0 * k0 * variance;
    const double new_p01 = b00 * a10 + b01 + k0 * k1 * variance;
    const double new_p10 = b10 * a00 + k1 * k0 * variance;
    const double new_p11 = b10 * a10 + b11 + k1 * k1 * variance;
    p00_ = std::max(new_p00, 1.0e-10);
    p01_ = 0.5 * (new_p01 + new_p10);
    p10_ = p01_;
    p11_ = std::max(new_p11, 1.0e-10);
  }

  // 저정밀 encoder와 learned measurement를 결합하고, 공분산 배율과 OOD/NIS gate를 갱신한다.
  // 모든 계산은 크기가 고정된 scalar/2x2 연산이므로 한 callback의 계산량이 O(1)이다.
  void on_sample(const daily_robotics_2026_10_23::msg::ActuatorSample & sample)
  {
    const auto cycle_start = std::chrono::steady_clock::now();

    if (!initialized_) {
      position_ = sample.encoder_position;
      velocity_ = 0.0;
      initialized_ = true;
    } else {
      // 등속 모델 x_k=F*x_{k-1}, F=[[1,dt],[0,1]]로 상태를 예측한다.
      position_ += kDt * velocity_;

      // P_k^-=F P F^T+Q를 2x2 원소로 전개했다.
      // Q는 실제 acceleration 불확실성을 흉내 내며 covariance collapse를 막는다.
      constexpr double q_position = 2.0e-6;
      constexpr double q_velocity = 8.0e-4;
      const double predicted_p00 =
        p00_ + kDt * (p01_ + p10_) + kDt * kDt * p11_ + q_position;
      const double predicted_p01 = p01_ + kDt * p11_;
      const double predicted_p10 = p10_ + kDt * p11_;
      const double predicted_p11 = p11_ + q_velocity;
      p00_ = predicted_p00;
      p01_ = predicted_p01;
      p10_ = predicted_p10;
      p11_ = predicted_p11;
    }

    // encoder는 learned sensor가 OOD여도 남는 fallback 관측이다.
    update_position(sample.encoder_position, kEncoderVariance);

    const double learned_innovation = sample.learned_position - position_;
    const double raw_innovation_variance = p00_ + sample.learned_variance;
    const double raw_nis =
      learned_innovation * learned_innovation / raw_innovation_variance;
    const bool is_ood = sample.ood_score > kOodThreshold;

    if (!is_ood && std::isfinite(raw_nis)) {
      // NIS가 1보다 계속 크면 모델이 보고한 R이 낙관적이라는 뜻이다.
      // EWMA와 [0.5,25] clip은 한 샘플이 배율을 폭주시킬 수 없게 한다.
      const double bounded_target = std::clamp(raw_nis, 0.5, 25.0);
      calibration_scale_ = std::clamp(
        (1.0 - kScaleLearningRate) * calibration_scale_ +
        kScaleLearningRate * bounded_target,
        0.5, 25.0);
    }

    // 논문의 P_calibrated=phi(P_estimated)를 가장 단순한 양의 scalar map으로 축약했다.
    // 실제 제품에서는 offline validation으로 학습한 map을 쓰고 online 값은 감시 신호로 둔다.
    const double calibrated_variance = std::clamp(
      calibration_scale_ * sample.learned_variance, 1.0e-6, 5.0e-2);
    const double learned_innovation_variance = p00_ + calibrated_variance;
    const double learned_nis =
      learned_innovation * learned_innovation / learned_innovation_variance;

    bool accepted = false;
    if (is_ood) {
      ++ood_reject_count_;
    } else if (!std::isfinite(learned_nis) || learned_nis > kNisThreshold) {
      ++nis_reject_count_;
    } else {
      update_position(sample.learned_position, calibrated_variance);
      accepted = true;
    }

    const auto cycle_end = std::chrono::steady_clock::now();
    const double cycle_time_us =
      std::chrono::duration<double, std::micro>(cycle_end - cycle_start).count();

    daily_robotics_2026_10_23::msg::EstimatorOutput output;
    output.header = sample.header;
    output.sequence = sample.sequence;
    output.raw_effort = sample.raw_effort;
    output.position = position_;
    output.velocity = velocity_;
    output.position_variance = p00_;
    output.calibration_scale = calibration_scale_;
    output.learned_nis = learned_nis;
    output.learned_measurement_accepted = accepted;
    output.ood_reject_count = ood_reject_count_;
    output.nis_reject_count = nis_reject_count_;
    output.cycle_time_us = cycle_time_us;
    output_publisher_->publish(output);

    if (sample.sequence % 200U == 0U) {
      RCLCPP_INFO(
        get_logger(),
        "estimate seq=%u q=%.3f qdot=%.3f scale=%.2f NIS=%.2f reject(ood/nis)=%u/%u",
        sample.sequence, position_, velocity_, calibration_scale_, learned_nis,
        ood_reject_count_, nis_reject_count_);
    }
  }

  rclcpp::Publisher<daily_robotics_2026_10_23::msg::EstimatorOutput>::SharedPtr
    output_publisher_;
  rclcpp::Subscription<daily_robotics_2026_10_23::msg::ActuatorSample>::SharedPtr
    sample_subscription_;

  bool initialized_{false};
  double position_{0.0};
  double velocity_{0.0};
  double p00_{0.10};
  double p01_{0.0};
  double p10_{0.0};
  double p11_{0.20};
  double calibration_scale_{1.0};
  std::uint32_t ood_reject_count_{0U};
  std::uint32_t nis_reject_count_{0U};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor가 filter callback을 직렬 실행하므로 P와 x의 동시 수정이 없다.
  rclcpp::spin(std::make_shared<AdaptiveCovarianceFilter>());
  rclcpp::shutdown();
  return 0;
}
