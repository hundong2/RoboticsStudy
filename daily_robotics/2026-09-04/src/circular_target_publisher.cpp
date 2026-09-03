#include <chrono>
#include <cmath>
#include <memory>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

// 이 노드는 IK 제어기에 도달 가능한 원형 목표점을 주기적으로 공급한다.
// 실제 로봇에서는 비전, 조이스틱, MoveIt 태스크 플래너가 이 역할을 맡을 수 있다.
class CircularTargetPublisher final : public rclcpp::Node
{
public:
  CircularTargetPublisher()
  : Node("circular_target_publisher")
  {
    // KeepLast(1)은 IK가 오래된 목표를 쌓아 두지 않고 가장 최신 목표만 보게 한다.
    // reliable()은 작은 제어 명령 예제에서 목표점 유실을 피하려는 선택이다.
    const auto target_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().durability_volatile();

    // geometry_msgs::msg::PointStamped는 좌표와 기준 프레임/시각을 한 메시지에 담는다.
    target_publisher_ = create_publisher<geometry_msgs::msg::PointStamped>(
      "/arm/target", target_qos);

    // create_wall_timer는 ROS 시간 변경과 무관한 steady wall clock으로 500 ms마다 호출한다.
    // 콜백은 rclcpp::spin이 executor에서 타이머를 실행할 때 실제로 수행된다.
    timer_ = create_wall_timer(500ms, [this]() {publish_next_target();});
  }

private:
  // base_link 평면에서 움직이는 목표점을 만들어 IK 입력으로 발행한다.
  void publish_next_target()
  {
    geometry_msgs::msg::PointStamped target;

    // now()는 이 노드의 clock(기본은 시스템 시간)을 ROS builtin_interfaces/Time으로 변환한다.
    target.header.stamp = now();
    // frame_id는 아래 x,y가 어느 좌표계에서 표현됐는지 명시한다.
    target.header.frame_id = "base_link";

    // x = 0.45 + 0.20 cos(t), y = 0.20 sin(t): 링크 길이 0.5 m, 0.4 m인
    // 2R 팔의 도달 반경 |L1-L2| <= r <= L1+L2 안쪽을 도는 원이다.
    target.point.x = 0.45 + 0.20 * std::cos(phase_rad_);
    target.point.y = 0.20 * std::sin(phase_rad_);
    target.point.z = 0.10;

    // publish는 메시지를 DDS/RMW 계층으로 넘긴다. 여기서는 읽기 쉬운 일반 publish를 쓰며,
    // 하드 RT 경로라면 loaned message 지원과 할당 발생 여부를 별도로 측정해야 한다.
    target_publisher_->publish(target);

    RCLCPP_INFO(
      get_logger(), "target(base_link): x=%.3f, y=%.3f", target.point.x, target.point.y);

    phase_rad_ += 0.20;
    constexpr double two_pi = 6.28318530717958647692;
    if (phase_rad_ >= two_pi) {
      phase_rad_ -= two_pi;
    }
  }

  rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr target_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  double phase_rad_{0.0};
};

int main(int argc, char ** argv)
{
  // init은 DDS 컨텍스트, ROS 인자 처리, 종료 신호 핸들러를 준비한다.
  rclcpp::init(argc, argv);
  auto node = std::make_shared<CircularTargetPublisher>();

  // spin은 기본 SingleThreadedExecutor에서 타이머가 준비되기를 기다렸다가 콜백을 실행한다.
  // 오늘의 IK 노드는 이 자동 실행 대신 WaitSet으로 take 순서를 직접 제어한다.
  rclcpp::spin(node);

  // shutdown은 컨텍스트를 닫고 대기 중인 ROS 엔티티를 깨워 안전하게 종료시킨다.
  rclcpp::shutdown();
  return 0;
}
