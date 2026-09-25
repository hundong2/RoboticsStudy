#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

namespace daily_robotics
{

// 이 노드는 상위 자세/보행 제어기가 만들 법한 목표 centroidal wrench를 재현한다.
// 실제 로봇에서는 상태 추정기와 모션 제어기가 계산한 값이 이 Topic의 입력이 된다.
class WrenchCommandPublisher final : public rclcpp::Node
{
public:
  WrenchCommandPublisher()
  : Node("wrench_command_publisher")
  {
    // KeepLast(1)은 접촉력 할당기가 오래된 명령을 순서대로 처리하지 않고 최신 목표만 보게 한다.
    // reliable()은 저주파 명령을 가능한 한 유실 없이 전달하려는 제어 명령용 선택이다.
    const auto command_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
    publisher_ = create_publisher<geometry_msgs::msg::WrenchStamped>(
      "/wbc/desired_wrench", command_qos);

    // 50 ms wall timer는 20 Hz 상위 명령 주기를 모사한다. 아래 200 Hz QP 커널보다 의도적으로 느리다.
    using namespace std::chrono_literals;
    timer_ = create_wall_timer(50ms, [this]() {publish_command();});
  }

private:
  // base_link 원점(질량중심)에서 로봇 전체가 만들어야 할 평면 wrench [Fx, Fz, tau_y]를 발행한다.
  void publish_command()
  {
    geometry_msgs::msg::WrenchStamped message;
    // now()는 ROS clock을 사용한다. 실기에서는 이 stamp로 명령 지연과 stale 여부를 검사해야 한다.
    message.header.stamp = now();
    // Wrench의 수치가 어느 좌표축 기준인지 반드시 명시한다. 여기서는 질량중심 base_link 기준이다.
    message.header.frame_id = "base_link";

    // 위상은 메시지 번호로부터 만들기 때문에 wall-clock 점프와 무관하게 재현 가능하다.
    const double phase = 0.08 * static_cast<double>(sequence_);

    // Wrench.force는 선형 힘 [N], Wrench.torque는 모멘트 [N·m]를 의미한다.
    // 20 kg 로봇의 정적 중량 mg = 20 * 9.81 = 196.2 N을 수직 목표로 둔다.
    message.wrench.force.x = 28.0 * std::sin(phase);
    message.wrench.force.y = 0.0;  // 오늘 예제는 x-z 평면 문제이므로 측방향 힘은 사용하지 않는다.
    message.wrench.force.z = 196.2;
    message.wrench.torque.x = 0.0;
    // pitch 모멘트를 함께 흔들어 두 발의 수직 하중이 실제로 이동하는지 확인한다.
    message.wrench.torque.y = 10.0 * std::sin(0.5 * phase);
    message.wrench.torque.z = 0.0;

    // publish는 DDS로 메시지를 넘긴다. 이 노드는 비실시간 관리 경로이므로 일반 ROS publish가 적절하다.
    publisher_->publish(message);
    ++sequence_;

    // THROTTLE 로그는 매 주기 콘솔 I/O가 제어 흐름을 방해하지 않도록 2초에 한 번만 출력한다.
    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 2000,
      "desired wrench Fx=%.2f N, Fz=%.2f N, tau_y=%.2f Nm",
      message.wrench.force.x, message.wrench.force.z, message.wrench.torque.y);
  }

  // SharedPtr는 ROS 실행기가 Publisher와 Timer 수명을 안전하게 관리하는 표준 C++ 스마트 포인터다.
  rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::uint64_t sequence_{0U};
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  // init은 DDS/RMW와 ROS 명령행 인자를 초기화한다. Node 생성 전에 정확히 한 번 호출해야 한다.
  rclcpp::init(argc, argv);
  // spin은 subscription/timer callback이 실행될 수 있도록 executor event loop를 계속 돌린다.
  rclcpp::spin(std::make_shared<daily_robotics::WrenchCommandPublisher>());
  // shutdown은 DDS participant와 ROS 자원을 정리한다.
  rclcpp::shutdown();
  return 0;
}
