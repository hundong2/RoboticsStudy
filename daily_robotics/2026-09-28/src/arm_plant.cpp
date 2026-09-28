#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"

#include "daily_robotics_2026_09_28/arm_model.hpp"

using namespace std::chrono_literals;

namespace study = daily_robotics_2026_09_28::model;

// 이 노드는 실제 하드웨어 대신 3-DoF 평면 팔의 동역학을 500 Hz로 적분한다.
// controller가 보낸 토크를 적용하고, 엔코더/속도 센서를 흉내 낸 JointState를 되돌려준다.
class ArmPlant : public rclcpp::Node
{
public:
  ArmPlant()
  : Node("arm_plant"),
    q_{0.35, -0.75, 0.55},
    q_dot_{0.0, 0.0, 0.0},
    commanded_torque_{0.0, 0.0, 0.0},
    applied_torque_{0.0, 0.0, 0.0},
    last_command_time_(std::chrono::steady_clock::now())
  {
    // SensorDataQoS는 센서 계열 데이터에 맞춘 best-effort/작은 큐 프로파일이다.
    // 오래된 상태를 쌓기보다 가장 최근 샘플을 제어기에 빨리 전달하려고 depth를 1로 제한한다.
    const auto state_qos = rclcpp::SensorDataQoS().keep_last(1);
    state_publisher_ = create_publisher<sensor_msgs::msg::JointState>(
      "/arm/joint_states", state_qos);

    // 토크 명령도 최신 값만 의미가 있으므로 best-effort depth 1을 사용한다.
    // 실제 제품은 ros2_control command interface와 하드웨어 watchdog을 추가해야 한다.
    const auto command_qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    torque_subscription_ = create_subscription<std_msgs::msg::Float64MultiArray>(
      "/arm/torque_command", command_qos,
      std::bind(&ArmPlant::on_torque_command, this, std::placeholders::_1));

    // vector 필드를 주기 콜백 밖에서 한 번만 크기 지정해 반복 resize를 피한다.
    joint_state_message_.name = {"joint_1", "joint_2", "joint_3"};
    joint_state_message_.position.resize(3);
    joint_state_message_.velocity.resize(3);
    joint_state_message_.effort.resize(3);

    // wall timer는 시뮬레이션 적분 주기 2 ms(500 Hz)를 만든다.
    // rclcpp::spin의 단일 스레드 executor 아래에서는 명령 콜백과 이 타이머가 동시에 실행되지 않는다.
    integration_timer_ = create_wall_timer(2ms, std::bind(&ArmPlant::integrate_and_publish, this));

    RCLCPP_INFO(get_logger(), "3-DoF arm plant started at 500 Hz");
  }

private:
  // 토크 topic을 받아 로봇 동역학에 적용할 3개 관절 토크로 복사한다.
  void on_torque_command(const std_msgs::msg::Float64MultiArray::SharedPtr message)
  {
    // Float64MultiArray는 길이가 schema로 고정되지 않으므로 소비자가 반드시 검사해야 한다.
    if (message->data.size() != 3U) {
      ++rejected_commands_;
      return;
    }
    for (std::size_t joint = 0; joint < 3; ++joint) {
      if (!std::isfinite(message->data[joint])) {
        ++rejected_commands_;
        return;
      }
    }
    for (std::size_t joint = 0; joint < 3; ++joint) {
      commanded_torque_[joint] = message->data[joint];
    }
    last_command_time_ = std::chrono::steady_clock::now();
    has_command_ = true;
  }

  // M(q)q_ddot + bias(q,q_dot) = tau를 풀어 한 스텝 적분하고 JointState를 발행한다.
  void integrate_and_publish()
  {
    constexpr double kDtSeconds = 0.002;
    constexpr double kTorqueLimitNm = 60.0;
    constexpr double kVelocityLimitRadPerSecond = 6.0;

    const auto steady_now = std::chrono::steady_clock::now();
    const auto command_age = steady_now - last_command_time_;
    const bool command_fresh = has_command_ && command_age <= 100ms;

    const study::Mat3 mass = study::mass_matrix(q_);
    const study::Vec3 bias = study::dynamics_bias(q_, q_dot_);
    study::Mat3 mass_inverse{};
    double mass_determinant = 0.0;
    const bool invertible = study::inverse3(mass, mass_inverse, mass_determinant);

    // 명령이 아직 없거나 100 ms 이상 끊기면 현재 자세를 지탱하는 bias 토크로 전환한다.
    // 이는 교육용 안전 fallback이며 실제 장비의 독립 STO/브레이크를 대신하지 않는다.
    for (std::size_t joint = 0; joint < 3; ++joint) {
      const double requested = command_fresh ? commanded_torque_[joint] : bias[joint];
      applied_torque_[joint] = std::clamp(requested, -kTorqueLimitNm, kTorqueLimitNm);
    }

    study::Vec3 acceleration{};
    if (invertible) {
      study::Vec3 generalized_force{};
      for (std::size_t joint = 0; joint < 3; ++joint) {
        generalized_force[joint] = applied_torque_[joint] - bias[joint];
      }
      for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
          acceleration[row] += mass_inverse[row][col] * generalized_force[col];
        }
      }
    } else {
      ++dynamics_failures_;
    }

    // semi-implicit Euler: 먼저 속도, 그 다음 위치를 갱신한다.
    // 명시적 Euler보다 기계계 에너지 거동이 나아 교육용 plant에서 안정적이다.
    for (std::size_t joint = 0; joint < 3; ++joint) {
      q_dot_[joint] = std::clamp(
        q_dot_[joint] + acceleration[joint] * kDtSeconds,
        -kVelocityLimitRadPerSecond, kVelocityLimitRadPerSecond);
      q_[joint] += q_dot_[joint] * kDtSeconds;
    }

    // Header.stamp는 이 상태 벡터가 유효한 ROS 시간이다. 배열 name/position/velocity/effort는
    // 같은 인덱스끼리 동일 관절을 나타내야 하므로 모두 정확히 3개로 유지한다.
    joint_state_message_.header.stamp = now();
    for (std::size_t joint = 0; joint < 3; ++joint) {
      joint_state_message_.position[joint] = q_[joint];
      joint_state_message_.velocity[joint] = q_dot_[joint];
      joint_state_message_.effort[joint] = applied_torque_[joint];
    }
    state_publisher_->publish(joint_state_message_);
  }

  study::Vec3 q_;
  study::Vec3 q_dot_;
  study::Vec3 commanded_torque_;
  study::Vec3 applied_torque_;
  std::chrono::steady_clock::time_point last_command_time_;
  bool has_command_{false};
  std::uint64_t rejected_commands_{0};
  std::uint64_t dynamics_failures_{0};

  sensor_msgs::msg::JointState joint_state_message_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr state_publisher_;
  rclcpp::Subscription<std_msgs::msg::Float64MultiArray>::SharedPtr torque_subscription_;
  rclcpp::TimerBase::SharedPtr integration_timer_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 DDS/RMW와 ROS 인자 처리를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 subscription과 timer가 준비될 때 콜백을 하나씩 실행한다.
  // SingleThreadedExecutor 기본 동작 덕분에 이 예제의 공유 상태에는 별도 mutex가 필요 없다.
  rclcpp::spin(std::make_shared<ArmPlant>());
  rclcpp::shutdown();
  return 0;
}
