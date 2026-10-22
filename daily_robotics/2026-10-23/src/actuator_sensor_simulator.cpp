#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "daily_robotics_2026_10_23/msg/actuator_sample.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64.hpp"

namespace
{
constexpr double kDt = 0.005;       // 200 Hz plant/sample 주기 [s]
constexpr double kInertia = 1.0;    // 단일 회전축 등가 관성 [kg m^2]
constexpr double kDamping = 0.35;   // 점성 마찰 계수 [N m s/rad]
constexpr double kStiffness = 0.60; // 원점으로 복귀시키는 등가 강성 [N m/rad]
}  // namespace

class ActuatorSensorSimulator : public rclcpp::Node
{
public:
  ActuatorSensorSimulator()
  : Node("actuator_sensor_simulator")
  {
    // SensorDataQoS는 센서 샘플에서 오래된 데이터를 쌓기보다 최신 데이터를 우선하는
    // best-effort/작은 큐 정책이다. 실제 하드웨어 드라이버도 같은 의도를 명시해야 한다.
    sample_publisher_ = create_publisher<daily_robotics_2026_10_23::msg::ActuatorSample>(
      "/study/actuator_sample", rclcpp::SensorDataQoS());
    joint_state_publisher_ = create_publisher<sensor_msgs::msg::JointState>(
      "/joint_states", rclcpp::SensorDataQoS());

    // 안전 감독기가 계산한 최종 토크를 받는다. depth=1은 다음 plant tick에 필요한 값이
    // 오직 가장 최근 명령 하나이기 때문이다.
    safe_effort_subscription_ = create_subscription<std_msgs::msg::Float64>(
      "/study/safe_effort", rclcpp::QoS(1).best_effort(),
      [this](const std_msgs::msg::Float64 & message) {
        applied_effort_ = message.data;
      });

    // wall timer는 ROS simulation time이 멈춰도 교육용 plant가 계속 움직인다.
    // 실제 robot에서는 hardware read/write 주기 또는 RT 제어 thread가 이 역할을 맡는다.
    timer_ = create_wall_timer(
      std::chrono::milliseconds(5),
      std::bind(&ActuatorSensorSimulator::simulate_and_publish, this));
  }

private:
  // 단일 관절 plant를 적분하고 encoder/학습 센서/원시 토크 명령을 같은 sequence로 묶는다.
  // 실무에서는 이 노드가 motor driver, encoder driver, perception model로 분리될 수 있다.
  void simulate_and_publish()
  {
    ++sequence_;
    const double time = static_cast<double>(sequence_) * kDt;

    // 회전 운동방정식 I*q_ddot + b*q_dot + k*q = tau를 q_ddot에 대해 푼 식이다.
    // semi-implicit Euler(q_dot을 먼저, q를 나중에 갱신)는 단순 explicit Euler보다
    // 감쇠계의 에너지를 안정적으로 다루면서도 계산량이 일정하다.
    const double acceleration =
      (applied_effort_ - kDamping * velocity_ - kStiffness * position_) / kInertia;
    velocity_ += acceleration * kDt;
    position_ += velocity_ * kDt;

    // 정상 구간에도 큰 토크 변화와 지속 양의 일을 요구하여 rate/power/energy gate를
    // 실제로 통과시킨다. 500 tick 주기의 pulse는 smoke test를 재현 가능하게 만든다.
    raw_effort_ = 4.5 * std::sin(0.75 * time);
    const std::uint32_t phase = sequence_ % 500U;
    if (phase >= 90U && phase < 145U) {
      raw_effort_ += 9.0;
    } else if (phase >= 330U && phase < 365U) {
      raw_effort_ -= 8.0;
    }

    daily_robotics_2026_10_23::msg::ActuatorSample sample;
    sample.header.stamp = now();
    sample.header.frame_id = "joint1_encoder";
    sample.sequence = sequence_;
    sample.truth_position = position_;
    sample.truth_velocity = velocity_;

    // encoder는 항상 사용 가능한 저정밀 기준 센서다. 난수 대신 서로 다른 주파수의
    // 결정론적 성분을 사용해 모든 검증 실행이 같은 결과를 내도록 한다.
    sample.encoder_position = position_ +
      0.035 * std::sin(0.37 * static_cast<double>(sequence_)) +
      0.015 * std::cos(0.11 * static_cast<double>(sequence_));

    // learned_position은 카메라/신경망 회귀기의 출력을 단순화한 값이다.
    // 모델은 분산 2.5e-5(표준편차 5 mm)를 보고하지만 실제 정상 오차는 더 크므로
    // downstream 필터가 그대로 믿으면 과신하게 된다.
    const bool is_ood =
      (phase >= 250U && phase < 290U) || (phase >= 420U && phase < 445U);
    const double nominal_error =
      0.020 * std::sin(0.61 * static_cast<double>(sequence_)) +
      0.012 * std::cos(0.17 * static_cast<double>(sequence_));
    const double ood_bias = is_ood ? ((phase < 300U) ? 0.32 : -0.28) : 0.0;
    sample.learned_position = position_ + nominal_error + ood_bias;
    sample.learned_variance = 2.5e-5;
    sample.ood_score = is_ood ? 0.95 : 0.08 + 0.03 * std::sin(0.03 * time);
    sample.raw_effort = raw_effort_;
    sample_publisher_->publish(sample);

    sensor_msgs::msg::JointState joint_state;
    // JointState의 Header.stamp는 이 위치/속도/토크가 유효했던 측정시각이다.
    // frame_id는 관절 이름을 대신하지 않으며, name 배열과 각 값 배열의 index가 대응한다.
    joint_state.header = sample.header;
    joint_state.name.push_back("joint1");
    joint_state.position.push_back(sample.encoder_position);
    joint_state.velocity.push_back(velocity_);
    joint_state.effort.push_back(applied_effort_);
    joint_state_publisher_->publish(joint_state);

    if (sequence_ % 200U == 0U) {
      RCLCPP_INFO(
        get_logger(),
        "sample=%u q=%.3f qdot=%.3f raw=%.3f applied=%.3f",
        sequence_, position_, velocity_, raw_effort_, applied_effort_);
    }
  }

  rclcpp::Publisher<daily_robotics_2026_10_23::msg::ActuatorSample>::SharedPtr
    sample_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_state_publisher_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr safe_effort_subscription_;
  rclcpp::TimerBase::SharedPtr timer_;

  std::uint32_t sequence_{0U};
  double position_{0.0};
  double velocity_{0.0};
  double raw_effort_{0.0};
  double applied_effort_{0.0};
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 ROS argument, DDS context, signal handler를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 subscription/timer callback을 실행한다. 이 simulator는 하나의 callback thread만
  // 사용하므로 plant 상태에 mutex가 필요 없고 tick 사이 상태 전이가 직렬화된다.
  rclcpp::spin(std::make_shared<ActuatorSensorSimulator>());
  rclcpp::shutdown();
  return 0;
}
