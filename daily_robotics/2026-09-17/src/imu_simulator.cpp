#include <chrono>
#include <cstdint>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "rosgraph_msgs/msg/clock.hpp"
#include "sensor_msgs/msg/imu.hpp"

using namespace std::chrono_literals;

// IMU 모델: 100 Hz, 평면 z축 회전 0.3 rad/s, 중력 제거된 body-x 가속도 0.5 m/s².
// 여기서 /clock은 시뮬레이션된 측정 시각이며 노드 실행을 깨우는 벽시계와 구분한다.
class ImuSimulator final : public rclcpp::Node {
public:
  ImuSimulator() : Node("imu_simulator") {
    // /clock의 단일 발행자가 모든 use_sim_time 노드의 ROS 시각을 공급한다.
    clock_pub_ = create_publisher<rosgraph_msgs::msg::Clock>("/clock", rclcpp::QoS(10));
    // SensorDataQoS는 IMU처럼 최신 값이 중요한 센서 흐름에 맞춘 best-effort 설정이다.
    imu_pub_ = create_publisher<sensor_msgs::msg::Imu>("/imu/data", rclcpp::SensorDataQoS());
    // 벽시계 타이머이므로 /clock이 처음 0이거나 뒤로 점프해도 발행 자체는 멈추지 않는다.
    timer_ = create_wall_timer(10ms, [this]() { publish_sample(); });
  }

private:
  // 샘플 번호에서 측정 시각을 재생성하므로 실행마다 같은 Header.stamp가 나온다.
  void publish_sample() {
    constexpr std::uint32_t kSamplesPerCycle = 400;
    constexpr std::int64_t kStartNs = 10'000'000'000LL;
    constexpr std::int64_t kStepNs = 10'000'000LL;
    const auto sequence = sample_index_ % kSamplesPerCycle;
    const std::int64_t stamp_ns = kStartNs + static_cast<std::int64_t>(sequence) * kStepNs;
    // builtin_interfaces/Time은 초와 나노초 필드를 별도로 저장한다.
    builtin_interfaces::msg::Time stamp;
    stamp.sec = static_cast<std::int32_t>(stamp_ns / 1'000'000'000LL);
    stamp.nanosec = static_cast<std::uint32_t>(stamp_ns % 1'000'000'000LL);

    rosgraph_msgs::msg::Clock clock;
    clock.clock = stamp;
    clock_pub_->publish(clock);

    sensor_msgs::msg::Imu imu;
    // Header는 데이터가 실제로 측정된 시각과 frame을 표기한다. 콜백 도착 시각이 아니다.
    imu.header.stamp = stamp;
    imu.header.frame_id = "imu_link";
    // orientation_covariance[0] = -1은 orientation 추정치가 제공되지 않음을 뜻한다.
    imu.orientation_covariance[0] = -1.0;
    imu.angular_velocity.z = 0.3;
    imu.linear_acceleration.x = 0.5;
    imu_pub_->publish(imu);
    ++sample_index_;
    if (sequence == 0 && sample_index_ > 1) {
      RCLCPP_INFO(get_logger(), "simulation clock jumped backward; new cycle=%u",
                  sample_index_ / kSamplesPerCycle);
    }
  }

  rclcpp::Publisher<rosgraph_msgs::msg::Clock>::SharedPtr clock_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::uint32_t sample_index_{0};
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  // spin은 단일 executor에서 타이머 콜백을 순서대로 호출한다.
  rclcpp::spin(std::make_shared<ImuSimulator>());
  rclcpp::shutdown();
  return 0;
}
