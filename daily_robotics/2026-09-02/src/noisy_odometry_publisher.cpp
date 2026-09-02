#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <random>

#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

/// 실제 엔코더/GNSS를 대신해 사인파 위치와 가우시안 잡음을 100 Hz로 만드는 센서 모사 노드다.
class NoisyOdometryPublisher final : public rclcpp::Node
{
public:
  NoisyOdometryPublisher()
  : Node("noisy_odometry_publisher"),
    // ROS 파라미터로 노이즈와 운동 주파수를 노드 재컴파일 없이 바꿀 수 있다.
    noise_stddev_(declare_parameter<double>("noise_stddev", 0.35)),
    angular_frequency_hz_(declare_parameter<double>("motion_frequency_hz", 0.20)),
    random_engine_(static_cast<std::mt19937::result_type>(
        declare_parameter<int>("random_seed", 42))),
    noise_distribution_(0.0, noise_stddev_),
    start_time_(now())
  {
    // SensorDataQoS는 센서 스트림에 맞춰 best-effort와 작은 history를 기본 사용한다.
    // keep_last(5)는 소비자가 늦을 때 오래된 샘플을 무한히 쌓지 않도록 큐를 제한한다.
    const auto sensor_qos = rclcpp::SensorDataQoS().keep_last(5);
    noisy_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/sensors/noisy_odometry", sensor_qos);

    // ground truth는 관측기가 늦어도 샘플을 확실히 비교하도록 reliable QoS를 사용한다.
    const auto truth_qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
    truth_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/simulation/ground_truth", truth_qos);

    // wall timer는 ROS 시간 정지 여부와 무관하게 매 10 ms마다 센서를 모사한다.
    // 실제 하드웨어 드라이버에서는 장치 타임스탬프와 주기 정확도를 별도로 검증해야 한다.
    timer_ = create_wall_timer(10ms, std::bind(&NoisyOdometryPublisher::publish_sample, this));
  }

private:
  /// 1차원 왕복 운동의 참값과 잡음이 섞인 위치 측정값을 각각 ROS Topic으로 발행한다.
  void publish_sample()
  {
    const auto stamp = now();
    const double t_seconds = (stamp - start_time_).seconds();
    const double omega = 2.0 * kPi * angular_frequency_hz_;

    // 수식 연결: p(t)=A sin(omega*t), v(t)=A*omega*cos(omega*t)이다.
    const double true_position = kAmplitudeMeters * std::sin(omega * t_seconds);
    const double true_velocity = kAmplitudeMeters * omega * std::cos(omega * t_seconds);
    const double measured_position = true_position + noise_distribution_(random_engine_);

    nav_msgs::msg::Odometry noisy_message;
    noisy_message.header.stamp = stamp;
    noisy_message.header.frame_id = "map";
    noisy_message.child_frame_id = "base_link";
    noisy_message.pose.pose.position.x = measured_position;
    // 단위 쿼터니언 (x,y,z,w)=(0,0,0,1)은 회전이 없음을 뜻한다.
    noisy_message.pose.pose.orientation.w = 1.0;
    // pose covariance는 6x6 행렬을 행 우선으로 펴 놓은 배열이며 [0]은 x 위치 분산이다.
    noisy_message.pose.covariance[0] = noise_stddev_ * noise_stddev_;
    noisy_publisher_->publish(noisy_message);

    nav_msgs::msg::Odometry truth_message;
    truth_message.header = noisy_message.header;
    truth_message.child_frame_id = noisy_message.child_frame_id;
    truth_message.pose.pose.position.x = true_position;
    truth_message.pose.pose.orientation.w = 1.0;
    truth_message.twist.twist.linear.x = true_velocity;
    truth_publisher_->publish(truth_message);
  }

  static constexpr double kPi = 3.14159265358979323846;
  static constexpr double kAmplitudeMeters = 2.0;

  const double noise_stddev_;
  const double angular_frequency_hz_;
  std::mt19937 random_engine_;
  std::normal_distribution<double> noise_distribution_;
  rclcpp::Time start_time_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr noisy_publisher_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr truth_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace daily_robotics

int main(int argc, char * argv[])
{
  // rclcpp::init은 ROS 인자(--ros-args 등)를 해석하고 DDS 통신 계층을 초기화한다.
  rclcpp::init(argc, argv);
  auto node = std::make_shared<daily_robotics::NoisyOdometryPublisher>();
  // spin은 타이머와 통신 이벤트를 기다렸다가 준비된 콜백을 반복 실행한다.
  rclcpp::spin(node);
  // shutdown은 DDS 엔티티와 ROS 컨텍스트를 질서 있게 정리한다.
  rclcpp::shutdown();
  return 0;
}
