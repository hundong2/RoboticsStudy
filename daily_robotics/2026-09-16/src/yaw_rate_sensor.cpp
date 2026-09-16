#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

#include "geometry_msgs/msg/twist_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

namespace daily_robotics
{

// 이 함수는 두 가상 센서가 같은 로봇 회전 운동을 관측하도록 공통 ground-truth를 만든다.
// 서로 다른 두 주파수를 섞는 이유는 단일 정현파의 주기적 모호성 때문에 잘못된 시간 오프셋을
// 선택하는 문제를 줄이기 위해서다. 식은 ω(t)=0.65sin(2π·0.45t)+0.25sin(2π·1.10t) [rad/s]다.
double true_yaw_rate(const double time_seconds)
{
  constexpr double kPi = 3.14159265358979323846;
  return 0.65 * std::sin(2.0 * kPi * 0.45 * time_seconds) +
         0.25 * std::sin(2.0 * kPi * 1.10 * time_seconds);
}

class YawRateSensor final : public rclcpp::Node
{
public:
  // 이 노드는 IMU 또는 LiDAR를 흉내 내어, 같은 회전 운동을 서로 다른 rate/분산/clock offset으로
  // 발행한다. 실기에서는 이 자리를 실제 sensor driver가 담당한다.
  YawRateSensor()
  : Node("yaw_rate_sensor")
  {
    // declare_parameter<T>는 launch/CLI에서 값을 덮어쓸 수 있게 하면서 기본값과 타입을 등록한다.
    sensor_name_ = declare_parameter<std::string>("sensor_name", "imu");
    topic_ = declare_parameter<std::string>("topic", "/imu/yaw_rate");
    frame_id_ = declare_parameter<std::string>("frame_id", "imu_link");
    rate_hz_ = declare_parameter<double>("rate_hz", 200.0);
    stamp_offset_ms_ = declare_parameter<double>("stamp_offset_ms", 0.0);
    variance_ = declare_parameter<double>("variance", 0.0004);

    if (rate_hz_ <= 0.0 || variance_ <= 0.0) {
      throw std::invalid_argument("rate_hz and variance must be positive");
    }

    // SensorDataQoS는 센서 stream에서 오래된 재전송보다 최신 표본을 우선한다.
    // 일반적으로 best-effort/volatile/small depth이므로 발행자와 구독자가 같은 profile을 사용한다.
    publisher_ = create_publisher<geometry_msgs::msg::TwistWithCovarianceStamped>(
      topic_, rclcpp::SensorDataQoS().keep_last(5));

    // create_wall_timer는 wall clock으로 publish callback을 주기 실행한다. 실제 hardware driver에서는
    // timer 대신 device interrupt/read loop가 메시지를 만들 수 있다.
    const auto period = std::chrono::microseconds(
      static_cast<std::int64_t>(1'000'000.0 / rate_hz_));
    timer_ = create_wall_timer(period, std::bind(&YawRateSensor::publish_sample, this));

    RCLCPP_INFO(
      get_logger(), "%s publishes %s at %.1f Hz with stamp offset %.1f ms",
      sensor_name_.c_str(), topic_.c_str(), rate_hz_, stamp_offset_ms_);
  }

private:
  // 센서의 한 표본을 생성한다. 값은 실제 측정 시각(now)의 운동을 따르지만, Header.stamp에는
  // clock 오류를 모사한 offset을 더한다. 따라서 arrival time만 비교하면 이 오류를 찾을 수 없다.
  void publish_sample()
  {
    const rclcpp::Time measurement_time = now();
    const double phase_time = std::fmod(measurement_time.seconds(), 120.0);

    // 완전히 무작위인 noise 대신 시간의 결정적 고주파 성분을 사용하면 smoke test가 재현 가능하다.
    const double noise_scale = sensor_name_ == "imu" ? 0.012 : 0.030;
    const double noise_phase = sensor_name_ == "imu" ? 0.0 : 0.7;
    const double measured_rate =
      true_yaw_rate(phase_time) + noise_scale * std::sin(31.0 * phase_time + noise_phase);

    geometry_msgs::msg::TwistWithCovarianceStamped message;
    // Header.stamp는 callback 도착 시각이 아니라 "이 값이 관측된 시각"이라는 계약이다.
    // 여기서는 의도적으로 LiDAR clock에 +35 ms 오류를 넣어 online calibration 대상으로 삼는다.
    const auto offset_ns = static_cast<std::int64_t>(stamp_offset_ms_ * 1'000'000.0);
    const rclcpp::Time stamped_time(
      measurement_time.nanoseconds() + offset_ns, measurement_time.get_clock_type());
    // Jazzy의 rclcpp::Time은 builtin_interfaces/Time 명시 변환 연산자를 제공한다.
    message.header.stamp = static_cast<builtin_interfaces::msg::Time>(stamped_time);
    message.header.frame_id = frame_id_;

    // Twist의 angular.z는 오른손 좌표계에서 z축 yaw 각속도 [rad/s]다.
    message.twist.twist.angular.z = measured_rate;
    // 6×6 covariance의 순서는 [vx,vy,vz,wx,wy,wz]이며, index 35가 wz 분산이다.
    // 사용하지 않는 축은 큰 분산을 줘서 downstream이 신뢰하지 않게 한다.
    message.twist.covariance.fill(0.0);
    for (std::size_t diagonal = 0; diagonal < 5; ++diagonal) {
      message.twist.covariance[diagonal * 6 + diagonal] = 1.0e6;
    }
    message.twist.covariance[35] = variance_;

    publisher_->publish(message);
  }

  std::string sensor_name_;
  std::string topic_;
  std::string frame_id_;
  double rate_hz_{200.0};
  double stamp_offset_ms_{0.0};
  double variance_{0.0004};
  rclcpp::Publisher<geometry_msgs::msg::TwistWithCovarianceStamped>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  // rclcpp::init은 ROS argument, DDS/RMW context, signal handler를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 subscription/timer가 준비될 때 callback을 실행한다. 이 sensor node는 timer 하나뿐이므로
  // SingleThreadedExecutor의 직렬 실행이 가장 단순하고 충분하다.
  rclcpp::spin(std::make_shared<daily_robotics::YawRateSensor>());
  // shutdown은 DDS entity와 ROS context를 질서 있게 해제한다.
  rclcpp::shutdown();
  return 0;
}
