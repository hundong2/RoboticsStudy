#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64.hpp"

using namespace std::chrono_literals;

// 이 노드는 실제 모터 드라이버와 엔코더를 대신한다. 200 Hz 목표 속도와 JointState를
// 게시하고, 출력 저하와 통신 단절을 순서대로 주입해 안전 감독기를 재현 가능하게 시험한다.
class ActuatorSimulator final : public rclcpp::Node
{
public:
  ActuatorSimulator()
  : Node("actuator_simulator")
  {
    // KeepLast(1)은 제어기가 오래된 목표를 줄 세우지 않고 가장 최신 명령만 보게 한다.
    requested_command_publisher_ = create_publisher<std_msgs::msg::Float64>(
      "/actuator/requested_command", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());

    // 200 Hz의 정상 간격은 5 ms다. Deadline 15 ms는 세 주기 이상 새 측정이 없으면
    // 계약 위반으로 보고, MANUAL_BY_TOPIC 50 ms lease는 Publisher heartbeat를 감시한다.
    rclcpp::QoS state_qos(rclcpp::KeepLast(5));
    state_qos.reliable()
      .deadline(15ms)
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(50ms);

    rclcpp::PublisherOptions state_options;
    // PublisherEventCallbacks는 데이터 콜백이 아니라 DDS가 보고한 QoS 사건을 받는다.
    state_options.event_callbacks.deadline_callback =
      [this](rclcpp::QOSDeadlineOfferedInfo & info) {
        RCLCPP_WARN(
          get_logger(), "offered deadline missed: +%d total=%d",
          info.total_count_change, info.total_count);
      };
    state_options.event_callbacks.liveliness_callback =
      [this](rclcpp::QOSLivelinessLostInfo & info) {
        RCLCPP_WARN(
          get_logger(), "publisher liveliness lost: +%d total=%d",
          info.total_count_change, info.total_count);
      };

    joint_state_publisher_ = create_publisher<sensor_msgs::msg::JointState>(
      "/joint_states", state_qos, state_options);

    // create_wall_timer는 ROS 시각(/clock)이 멈춰도 실제 5 ms wall clock으로 시험을 진행한다.
    timer_ = create_wall_timer(5ms, std::bind(&ActuatorSimulator::on_timer, this));
    RCLCPP_INFO(
      get_logger(),
      "200 Hz bench: healthy[0,2), degraded[2,4.5), dropout[6.5,7.1) s");
  }

private:
  // 명령 프로파일, 1차 모터 동역학, 결정론적 잡음을 갱신하고 ROS 메시지를 게시한다.
  void on_timer()
  {
    constexpr double dt = 0.005;
    constexpr double motor_time_constant_s = 0.12;
    time_s_ += dt;

    // 12초마다 같은 시험을 반복한다. auditor는 첫 번째 주기만 사용한다.
    const double phase_s = std::fmod(time_s_, 12.0);
    const double requested_velocity = 5.0 + 1.2 * std::sin(0.70 * time_s_);
    const bool degraded = phase_s >= 2.0 && phase_s < 4.5;
    const bool dropout = phase_s >= 6.5 && phase_s < 7.1;
    const double control_effectiveness = degraded ? 0.25 : 1.0;

    std_msgs::msg::Float64 requested_message;
    requested_message.data = requested_velocity;
    requested_command_publisher_->publish(requested_message);

    // 실제 plant 모델: v_dot = (g*u-v)/tau. g=1은 정상, g=0.25는 75% 출력 저하다.
    // 전진 오일러로 v(k+1)=v(k)+dt/tau*(g*u(k)-v(k))를 계산한다.
    velocity_rad_s_ += (dt / motor_time_constant_s) *
      (control_effectiveness * requested_velocity - velocity_rad_s_);
    position_rad_ += velocity_rad_s_ * dt;

    if (degraded != degraded_active_) {
      degraded_active_ = degraded;
      RCLCPP_WARN(
        get_logger(), "loss-of-effectiveness %s",
        degraded_active_ ? "START (gain=0.25)" : "END (gain=1.0)");
    }
    if (dropout != dropout_active_) {
      dropout_active_ = dropout;
      RCLCPP_WARN(
        get_logger(), "JointState transport dropout %s",
        dropout_active_ ? "START" : "END");
    }

    // 통신 장애 구간에는 데이터와 수동 liveliness assertion을 모두 멈춘다.
    if (dropout) {
      return;
    }

    sensor_msgs::msg::JointState state_message;
    // header.stamp는 수신 시각이 아니라 엔코더 측정이 생성된 시각이라는 계약이다.
    state_message.header.stamp = now();
    state_message.header.frame_id = "actuator_bench";
    state_message.name = {"left_wheel_joint"};
    state_message.position = {position_rad_};
    // 난수 대신 사인 잡음을 사용해 빌드/실행마다 같은 회귀 결과를 얻는다.
    state_message.velocity = {velocity_rad_s_ + 0.02 * std::sin(31.0 * time_s_)};
    // effort에는 교육용 ground truth gain을 넣지만 감독기는 이 값을 읽지 않는다.
    state_message.effort = {control_effectiveness};
    joint_state_publisher_->publish(state_message);

    // MANUAL_BY_TOPIC 정책에서는 장치가 살아 있다는 lease를 명시적으로 갱신한다.
    if (!joint_state_publisher_->assert_liveliness()) {
      RCLCPP_WARN(get_logger(), "assert_liveliness() was rejected by the RMW");
    }
  }

  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr requested_command_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;

  double time_s_{0.0};
  double position_rad_{0.0};
  double velocity_rad_s_{0.0};
  bool degraded_active_{false};
  bool dropout_active_{false};
};

int main(int argc, char * argv[])
{
  // rclcpp::init은 ROS 인자, DDS/RMW 통신 계층, 프로세스 컨텍스트를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 5 ms Timer와 DDS QoS 이벤트를 준비되는 순서대로 실행한다.
  rclcpp::spin(std::make_shared<ActuatorSimulator>());
  rclcpp::shutdown();
  return 0;
}
