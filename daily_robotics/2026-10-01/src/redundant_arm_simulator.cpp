#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "geometry_msgs/msg/twist_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

#include "daily_robotics_2026_10_01/msg/joint_velocity_command.hpp"

using JointVelocityCommand = daily_robotics_2026_10_01::msg::JointVelocityCommand;

// 이 노드는 3R 평면 팔의 현재 관절각과 일부러 빠른 Cartesian 목표를 만든다.
// 로봇 시스템에서의 역할은 실제 encoder/상위 motion planner를 대신해 SNS 제어기를 반복 시험하는 것이다.
class RedundantArmSimulator : public rclcpp::Node
{
public:
  RedundantArmSimulator()
  : Node("redundant_arm_simulator"),
    start_time_(std::chrono::steady_clock::now())
  {
    // SensorDataQoS는 센서처럼 최신 샘플이 중요한 데이터에 쓰는 ROS 2 기본 프로필이다.
    // keep_last(1)로 오래된 관절 상태와 목표가 큐에 쌓이지 않게 한다.
    const auto stream_qos = rclcpp::SensorDataQoS().keep_last(1);
    joint_state_publisher_ = create_publisher<sensor_msgs::msg::JointState>(
      "/joint_states", stream_qos);
    target_publisher_ = create_publisher<geometry_msgs::msg::TwistStamped>(
      "/cartesian_velocity_target", stream_qos);

    // 관절 명령도 최신 값 하나만 유효하다. best_effort depth 1은 지연된 명령 재생보다 유실을 택한다.
    command_subscription_ = create_subscription<JointVelocityCommand>(
      "/joint_velocity_command",
      rclcpp::QoS(rclcpp::KeepLast(1)).best_effort(),
      std::bind(&RedundantArmSimulator::on_command, this, std::placeholders::_1));

    // JointState는 name과 position/velocity 배열의 같은 index가 같은 관절이어야 한다.
    // vector 저장공간을 생성자에서 한 번 준비해 100 Hz 콜백에서 resize하지 않는다.
    joint_state_message_.name = {"joint_1", "joint_2", "joint_3"};
    joint_state_message_.position.resize(3);
    joint_state_message_.velocity.resize(3);
    target_message_.header.frame_id = "base_link";

    // 3R 팔이 거의 펴진 상태에서 시작해 Jacobian 조건수가 커지는 구간도 의도적으로 만든다.
    position_rad_ = {0.15, -0.28, 0.12};

    // create_wall_timer는 ROS simulation time과 무관한 steady wall clock 기반 타이머다.
    // 10 ms 주기는 100 Hz encoder/목표 생성기를 흉내 낸다.
    timer_ = create_wall_timer(
      std::chrono::milliseconds(10),
      std::bind(&RedundantArmSimulator::on_timer, this));

    RCLCPP_INFO(get_logger(), "3R simulator ready: 100 Hz state/target, near-singular start");
  }

private:
  // 안전 제어기가 발행한 최신 관절 속도만 보관한다.
  // SingleThreadedExecutor에서 콜백이 직렬 실행되므로 이 예제에서는 mutex가 필요 없다.
  void on_command(const JointVelocityCommand::SharedPtr message)
  {
    bool finite = true;
    for (const double value : message->velocity_rad_s) {
      finite = finite && std::isfinite(value);
    }
    if (!finite) {
      commanded_velocity_rad_s_.fill(0.0);
      return;
    }
    commanded_velocity_rad_s_ = message->velocity_rad_s;
    last_command_sequence_ = message->sequence;
  }

  // 100 Hz마다 q[k+1] = q[k] + dt*q_dot[k]인 전진 Euler 적분으로 관절 상태를 갱신한다.
  // 실제 모터 동역학을 재현하는 모델이 아니라 제한 알고리즘의 폐루프 연결을 확인하는 teaching plant다.
  void on_timer()
  {
    constexpr double kDtSeconds = 0.01;
    // teaching plant에도 물리 관절 범위를 둔다. q2/q3의 기계적 stop은 완전 직선(rank 1) 자세로
    // 무한히 밀려가는 비현실적 적분을 막고, condition이 나쁜 영역은 그대로 남겨 guard를 시험한다.
    constexpr std::array<double, 3> kLowerPositionRad{-2.6, -1.4, 0.10};
    constexpr std::array<double, 3> kUpperPositionRad{2.6, -0.18, 1.40};
    for (std::size_t joint = 0; joint < position_rad_.size(); ++joint) {
      const double previous_position = position_rad_[joint];
      position_rad_[joint] = std::clamp(
        previous_position + kDtSeconds * commanded_velocity_rad_s_[joint],
        kLowerPositionRad[joint], kUpperPositionRad[joint]);
      // stop에 닿아 실제로 움직이지 못한 경우 JointState.velocity도 명령값이 아니라 실현 속도를 보고한다.
      applied_velocity_rad_s_[joint] =
        (position_rad_[joint] - previous_position) / kDtSeconds;
    }

    // now()는 이 노드의 ROS clock을 읽는다. 같은 stamp를 두 메시지에 써서 동시 샘플임을 표시한다.
    const auto stamp = now();
    joint_state_message_.header.stamp = stamp;
    for (std::size_t joint = 0; joint < position_rad_.size(); ++joint) {
      joint_state_message_.position[joint] = position_rad_[joint];
      joint_state_message_.velocity[joint] = applied_velocity_rad_s_[joint];
    }
    joint_state_publisher_->publish(joint_state_message_);

    const double elapsed_seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - start_time_).count();
    target_message_.header.stamp = stamp;

    // 일부러 관절 한계가 허용하기 어려운 속도를 넣어 SNS가 redundancy를 먼저 쓰고,
    // 그래도 불가능하면 x_dot_safe = s*x_dot_desired로 방향을 보존하며 줄이게 한다.
    target_message_.twist.linear.x = 0.75 + 0.55 * std::sin(0.55 * elapsed_seconds);
    target_message_.twist.linear.y = 0.65 * std::cos(0.37 * elapsed_seconds);
    target_message_.twist.linear.z = 0.0;
    target_message_.twist.angular.x = 0.0;
    target_message_.twist.angular.y = 0.0;
    target_message_.twist.angular.z = 0.0;
    target_publisher_->publish(target_message_);

    if ((tick_count_++ % 500U) == 0U) {
      RCLCPP_INFO(
        get_logger(), "plant q=[%.3f %.3f %.3f], latest command seq=%llu",
        position_rad_[0], position_rad_[1], position_rad_[2],
        static_cast<unsigned long long>(last_command_sequence_));
    }
  }

  std::chrono::steady_clock::time_point start_time_;
  std::array<double, 3> position_rad_{};
  std::array<double, 3> commanded_velocity_rad_s_{};
  std::array<double, 3> applied_velocity_rad_s_{};
  std::uint64_t last_command_sequence_{0};
  std::uint64_t tick_count_{0};
  sensor_msgs::msg::JointState joint_state_message_;
  geometry_msgs::msg::TwistStamped target_message_;

  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr target_publisher_;
  rclcpp::Subscription<JointVelocityCommand>::SharedPtr command_subscription_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 ROS 인자, DDS context, signal handler를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 executor event loop를 돌며 subscription과 timer callback을 준비된 순서대로 실행한다.
  rclcpp::spin(std::make_shared<RedundantArmSimulator>());
  rclcpp::shutdown();
  return 0;
}
