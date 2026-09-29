#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_09_30/msg/contact_estimate.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

// ContactCovarianceEstimator의 역할:
// 같은 시각의 foot force와 foot velocity를 결합해 접촉 확률, 속도 분산, 보수적 마찰 하한을 계산한다.
// 논문의 대규모 learned covariance를 그대로 복제하지 않고, 원리를 볼 수 있는 bounded scalar kernel로 축약했다.
class ContactCovarianceEstimator : public rclcpp::Node
{
public:
  ContactCovarianceEstimator()
  : Node("contact_covariance_estimator")
  {
    // 두 센서 subscription이 같은 SensorDataQoS여야 DDS QoS 비호환으로 연결이 끊기지 않는다.
    const auto sensor_qos = rclcpp::SensorDataQoS().keep_last(5);
    wrench_subscription_ = create_subscription<geometry_msgs::msg::WrenchStamped>(
      "/foot/wrench", sensor_qos,
      [this](const geometry_msgs::msg::WrenchStamped::ConstSharedPtr message) {
        latest_wrench_ = *message;
        have_wrench_ = true;
      });
    velocity_subscription_ = create_subscription<geometry_msgs::msg::TwistStamped>(
      "/foot/twist", sensor_qos,
      [this](const geometry_msgs::msg::TwistStamped::ConstSharedPtr message) {
        on_velocity(message);
      });

    // 추정 결과는 안전 감독기가 반드시 받아야 하므로 reliable을 선택한다.
    estimate_publisher_ = create_publisher<daily_robotics_2026_09_30::msg::ContactEstimate>(
      "/contact/estimate", rclcpp::QoS(10).reliable());
  }

private:
  static double clamp(const double value, const double low, const double high)
  {
    return std::max(low, std::min(value, high));
  }

  void on_velocity(const geometry_msgs::msg::TwistStamped::ConstSharedPtr & velocity)
  {
    const auto callback_start = std::chrono::steady_clock::now();
    if (!have_wrench_) {
      ++rejected_pairs_;
      return;
    }

    // ROS Time을 정수 nanosecond로 바꿔 부동소수점 timestamp 비교 오차를 피한다.
    const std::int64_t wrench_ns = rclcpp::Time(latest_wrench_.header.stamp).nanoseconds();
    const std::int64_t velocity_ns = rclcpp::Time(velocity->header.stamp).nanoseconds();
    const double pair_skew_ms =
      std::abs(static_cast<double>(wrench_ns - velocity_ns)) * 1.0e-6;
    if (pair_skew_ms > 3.0 || velocity_ns == last_velocity_stamp_ns_) {
      ++rejected_pairs_;
      return;
    }
    last_velocity_stamp_ns_ = velocity_ns;

    const double normal_force = std::max(0.0, latest_wrench_.wrench.force.z);
    const double tangential_force = std::hypot(
      latest_wrench_.wrench.force.x, latest_wrench_.wrench.force.y);
    const double slip_speed = std::hypot(velocity->twist.linear.x, velocity->twist.linear.y);

    // p(contact)=1/(1+exp(-(F_n-20)/6)): 20 N 부근을 부드럽게 넘는 연속 접촉 신뢰도다.
    // 이진 threshold와 달리 경계 부근 불확실성을 뒤의 covariance에 전달할 수 있다.
    const double contact_probability = 1.0 / (1.0 + std::exp(-(normal_force - 20.0) / 6.0));
    const double friction_ratio = tangential_force / std::max(normal_force, 1.0);

    if (contact_probability > 0.7) {
      // 미끄럼이 없으면 현재 이용률보다 마찰 한계가 높다는 증거(+0.40), 미끄러지면 약간 낮다는 증거(-0.03)다.
      const bool slipping = slip_speed > 0.02;
      const double evidence = clamp(
        friction_ratio + (slipping ? -0.03 : 0.40), 0.05, 1.20);
      const double alpha = slipping ? 0.35 : 0.03;
      const double residual = evidence - friction_mean_;
      friction_mean_ += alpha * residual;
      // sigma^2 <- (1-alpha)sigma^2 + alpha*residual^2: 고정 메모리 EWMA 분산이다.
      friction_variance_ =
        (1.0 - alpha) * friction_variance_ + alpha * residual * residual;
    }

    const double friction_stddev = std::sqrt(std::max(friction_variance_, 4.0e-4));
    // 2-sigma 하한을 안전 한계로 쓴다. Gaussian 가정이 깨질 수 있어 이것만으로 안전 인증할 수는 없다.
    const double friction_lower = clamp(friction_mean_ - 2.0 * friction_stddev, 0.05, 1.20);
    // Coulomb margin = mu_lower*F_n-|F_t|. 음수면 보수적 friction cone 바깥이다.
    const double friction_margin = friction_lower * normal_force - tangential_force;
    // 접촉이 불확실하거나 발이 움직이면 속도 측정 covariance를 크게 만든다.
    const double velocity_variance =
      1.0e-5 + (1.0 - contact_probability) * 0.04 + slip_speed * slip_speed;

    std::uint8_t mode = 1U;
    if (contact_probability < 0.5) {
      mode = 0U;
    } else if (slip_speed > 0.025 || friction_margin < 0.0) {
      mode = 2U;
    }

    ++accepted_samples_;
    daily_robotics_2026_09_30::msg::ContactEstimate estimate;
    estimate.header = velocity->header;
    estimate.mode = mode;
    estimate.normal_force_n = normal_force;
    estimate.tangential_force_n = tangential_force;
    estimate.slip_speed_mps = slip_speed;
    estimate.contact_probability = contact_probability;
    estimate.velocity_variance = velocity_variance;
    estimate.friction_ratio = friction_ratio;
    estimate.friction_mean = friction_mean_;
    estimate.friction_stddev = friction_stddev;
    estimate.friction_lower_bound = friction_lower;
    estimate.friction_margin_n = friction_margin;
    estimate.pair_skew_ms = pair_skew_ms;
    estimate.callback_time_us = std::chrono::duration<double, std::micro>(
      std::chrono::steady_clock::now() - callback_start).count();
    estimate.accepted_samples = accepted_samples_;
    estimate.rejected_pairs = rejected_pairs_;
    estimate_publisher_->publish(estimate);
  }

  geometry_msgs::msg::WrenchStamped latest_wrench_{};
  bool have_wrench_{false};
  std::int64_t last_velocity_stamp_ns_{-1};
  double friction_mean_{0.75};
  double friction_variance_{0.01};
  std::uint32_t accepted_samples_{0U};
  std::uint32_t rejected_pairs_{0U};
  rclcpp::Subscription<geometry_msgs::msg::WrenchStamped>::SharedPtr wrench_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr velocity_subscription_;
  rclcpp::Publisher<daily_robotics_2026_09_30::msg::ContactEstimate>::SharedPtr
    estimate_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor의 순차 callback 실행은 latest_wrench_를 mutex 없이 일관되게 읽게 한다.
  rclcpp::spin(std::make_shared<ContactCovarianceEstimator>());
  rclcpp::shutdown();
  return 0;
}
