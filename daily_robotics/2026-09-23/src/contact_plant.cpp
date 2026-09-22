#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>

#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64.hpp"

using namespace std::chrono_literals;

namespace
{
// 실습 전체가 공유하는 1자유도 물리 모델이다. 실제 장비에서는 URDF/식별 결과에서 얻어야 한다.
constexpr double kDt = 0.002;              // 500 Hz 적분 주기 [s]
constexpr double kMass = 1.0;              // 이동축의 등가 질량 [kg]
constexpr double kJointDamping = 1.2;      // 관절 점성 마찰 [N·s/m]
constexpr double kWallPosition = 1.0;       // 강체 벽의 위치 [m]
constexpr double kWallStiffness = 250.0;    // 접촉 환경 강성 [N/m]
constexpr double kWallDamping = 4.0;        // 접촉 환경 감쇠 [N·s/m]
}  // namespace

/**
 * @brief 안전 게이트가 허용한 힘을 받아 1축 접촉 운동을 적분하는 교육용 플랜트 노드.
 *
 * 로봇 시스템에서 이 노드는 실제 모터 드라이버와 엔코더/힘 센서를 대신한다. 제어기의 원시 명령을
 * 직접 받지 않고 `/joint_effort_allowed`만 받으므로, 안전 감사기가 정지를 선언하면 플랜트 입력이
 * 반드시 0으로 바뀌는 구조를 코드 수준에서 확인할 수 있다.
 */
class ContactPlant final : public rclcpp::Node
{
public:
  ContactPlant()
  : Node("contact_plant"), start_(std::chrono::steady_clock::now())
  {
    // SensorDataQoS는 깊이 5, best-effort, volatile이다. 최신 센서 샘플이 오래된 샘플보다 중요한
    // 고주기 상태 스트림이므로 재전송 대기 대신 지연 상한을 낮추는 선택이다.
    joint_pub_ = create_publisher<sensor_msgs::msg::JointState>(
      "/contact/joint_state", rclcpp::SensorDataQoS());
    wrench_pub_ = create_publisher<geometry_msgs::msg::WrenchStamped>(
      "/contact/wrench", rclcpp::SensorDataQoS());

    // 목표는 20 Hz의 저주기 설정값이다. 유실 시 다음 목표를 기다리기보다 마지막 값이 중요하므로
    // reliable + keep_last(1)을 사용한다.
    target_pub_ = create_publisher<std_msgs::msg::Float64>(
      "/contact/target", rclcpp::QoS(1).reliable());

    // 감사기가 검사·제한한 명령만 구독한다. raw 명령 Topic을 구독하지 않는 것이 안전 경계의 핵심이다.
    allowed_effort_sub_ = create_subscription<std_msgs::msg::Float64>(
      "/joint_effort_allowed", rclcpp::QoS(4).reliable(),
      [this](const std_msgs::msg::Float64::SharedPtr msg) {
        allowed_effort_ = std::clamp(msg->data, -15.0, 15.0);
      });

    // JointState의 배열을 생성자에서 한 번만 1칸으로 잡아 타이머마다 resize하지 않는다.
    joint_msg_.name = {"contact_axis_joint"};
    joint_msg_.position.resize(1);
    joint_msg_.velocity.resize(1);
    joint_msg_.effort.resize(1);
    // WrenchStamped.header.frame_id는 힘이 tool0 좌표계에서 표현됐음을 뜻한다.
    wrench_msg_.header.frame_id = "tool0";

    // create_wall_timer는 ROS 시뮬레이션 시간과 무관한 steady wall clock으로 2 ms마다 콜백을 깨운다.
    // 교육용 일반 Linux에서는 평균 500 Hz일 뿐 hard RT 주기 보장은 아니다.
    physics_timer_ = create_wall_timer(2ms, std::bind(&ContactPlant::step_physics, this));
    target_timer_ = create_wall_timer(50ms, std::bind(&ContactPlant::publish_target, this));
  }

private:
  /** @brief 허용 힘과 접촉 반력을 합산해 질량-스프링-댐퍼 운동방정식을 한 스텝 적분한다. */
  void step_physics()
  {
    const double penetration = std::max(0.0, position_ - kWallPosition);

    // Kelvin-Voigt 접촉식 F_env = -K_env*x_pen - D_env*v를 구현한다.
    // unilateral 접촉은 벽이 로봇을 끌어당길 수 없으므로 min(0, ·)로 압축력만 남긴다.
    const double contact_force = penetration > 0.0 ?
      std::min(0.0, -kWallStiffness * penetration - kWallDamping * velocity_) : 0.0;

    // m*q_ddot = tau_allowed + F_env - b*q_dot 를 q_ddot에 대해 푼 식이다.
    const double acceleration =
      (allowed_effort_ + contact_force - kJointDamping * velocity_) / kMass;

    // semi-implicit Euler: 먼저 속도를, 그 새 속도로 위치를 갱신하면 명시적 Euler보다
    // 스프링 계의 수치 에너지가 덜 폭주한다. 실제 제어 검증에는 더 정교한 적분기가 필요하다.
    velocity_ += acceleration * kDt;
    position_ += velocity_ * kDt;

    const auto stamp = now();
    joint_msg_.header.stamp = stamp;
    joint_msg_.position[0] = position_;
    joint_msg_.velocity[0] = velocity_;
    joint_msg_.effort[0] = allowed_effort_;

    wrench_msg_.header.stamp = stamp;
    // 1축 직선 접촉을 x축 힘으로 표현한다. 나머지 힘/토크 성분은 기본값 0이다.
    wrench_msg_.wrench.force.x = contact_force;

    joint_pub_->publish(joint_msg_);
    wrench_pub_->publish(wrench_msg_);
  }

  /** @brief 자유공간→벽 접촉→후퇴 순서의 목표 위치를 발행해 한 번의 실습에서 세 구간을 만든다. */
  void publish_target()
  {
    const double elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - start_).count();
    std_msgs::msg::Float64 msg;
    // 1초 뒤 벽 너머 1.06 m를 명령해 임피던스가 약 4~5 N 접촉력을 자연스럽게 만들게 한다.
    msg.data = elapsed < 1.0 ? 0.55 : (elapsed < 6.0 ? 1.06 : 0.65);
    target_pub_->publish(msg);
  }

  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_pub_;
  rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr wrench_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr target_pub_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr allowed_effort_sub_;
  rclcpp::TimerBase::SharedPtr physics_timer_;
  rclcpp::TimerBase::SharedPtr target_timer_;

  sensor_msgs::msg::JointState joint_msg_;
  geometry_msgs::msg::WrenchStamped wrench_msg_;
  std::chrono::steady_clock::time_point start_;
  double position_{0.20};
  double velocity_{0.0};
  double allowed_effort_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // spin은 DDS 수신과 두 타이머를 현재 스레드에서 순차 실행한다. 콜백 공유 상태에 mutex가 필요 없지만,
  // 긴 콜백 하나가 다른 콜백을 지연시키므로 각 콜백은 반드시 작고 bounded해야 한다.
  rclcpp::spin(std::make_shared<ContactPlant>());
  rclcpp::shutdown();
  return 0;
}
