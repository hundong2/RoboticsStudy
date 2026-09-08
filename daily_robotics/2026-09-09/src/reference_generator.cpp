#include <chrono>
#include <functional>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"

using namespace std::chrono_literals;

// 이 노드는 시험용 관절 목표를 번갈아 발행한다.
// 실제 로봇에서는 조이스틱, 경로 계획기, 상위 행동 제어기가 이 역할을 맡는다.
class ReferenceGenerator final : public rclcpp::Node
{
public:
  ReferenceGenerator()
  : Node("reference_generator")
  {
    // KeepLast(1)은 제어기가 최신 목표 하나만 필요하다는 뜻이다.
    // reliable()은 드문 목표 변경을 유실하지 않게 하고, transient_local()은 제어기가 늦게
    // 시작해도 Publisher가 보관한 마지막 목표를 즉시 받을 수 있게 한다.
    const auto target_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    target_publisher_ = create_publisher<std_msgs::msg::Float64>("/joint/target", target_qos);

    // Wall timer는 ROS simulation time이 멈춰도 실제 벽시계 기준으로 2초마다 호출된다.
    // 학습 데모가 일정하게 왕복하도록 목표를 +0.8 rad와 -0.6 rad 사이에서 전환한다.
    timer_ = create_wall_timer(2s, std::bind(&ReferenceGenerator::publish_next_target, this));

    // 첫 timer 만료를 기다리지 않고 초기 목표를 발행한다. transient_local QoS 덕분에
    // 이후에 시작한 MPC 노드도 이 메시지를 받을 수 있다.
    publish_target(0.8);
  }

private:
  // 상위 명령을 std_msgs/Float64 메시지로 감싸 /joint/target Topic에 발행한다.
  void publish_target(const double target_rad)
  {
    std_msgs::msg::Float64 message;
    message.data = target_rad;
    target_publisher_->publish(message);
    RCLCPP_INFO(get_logger(), "새 관절 목표: %.3f rad", target_rad);
  }

  // 매 호출마다 목표 부호와 크기를 바꿔 step 응답과 제약 동작을 관찰하게 한다.
  void publish_next_target()
  {
    positive_target_ = !positive_target_;
    publish_target(positive_target_ ? 0.8 : -0.6);
  }

  bool positive_target_{true};
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr target_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[])
{
  // init은 DDS/RMW와 ROS 인자 처리 등 rclcpp 전역 런타임을 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 현재 스레드에서 timer callback을 준비될 때마다 실행한다.
  rclcpp::spin(std::make_shared<ReferenceGenerator>());
  // shutdown은 Context와 DDS 자원을 정상적으로 정리한다.
  rclcpp::shutdown();
  return 0;
}
