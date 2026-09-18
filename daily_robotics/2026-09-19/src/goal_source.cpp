#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"

using namespace std::chrono_literals;

// 테스트용 관절 목표를 공급한다. 실제 모터 명령이 아니라 계획 요청이다.
class GoalSource final : public rclcpp::Node {
 public:
  GoalSource() : Node("goal_source") {
    // 신뢰성 RELIABLE/깊이 1: 오래된 목표를 쌓지 않고 마지막 요청만 기다린다.
    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
    publisher_ = create_publisher<std_msgs::msg::Float64>("/study/goal_rad", qos);
    // 벽시계 타이머이므로 시뮬레이션 /clock 정지와 무관하게 데모 목표를 보낸다.
    timer_ = create_wall_timer(7s, [this]() { publish_goal(); });
    // 시작 후 첫 목표를 일찍 내보내되, DDS 구독자 발견 시간을 허용한다.
    first_timer_ = create_wall_timer(1s, [this]() {
      publish_goal();
      first_timer_->cancel();
    });
  }

 private:
  // +1/-0.75 rad를 번갈아 보내 경로 연결과 방향 반전도 관찰한다.
  void publish_goal() {
    std_msgs::msg::Float64 msg;
    msg.data = next_positive_ ? 1.0 : -0.75;
    next_positive_ = !next_positive_;
    publisher_->publish(msg);
    RCLCPP_INFO(get_logger(), "goal_rad=%.3f", msg.data);
  }

  bool next_positive_{true};
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_, first_timer_;
};

int main(int argc, char **argv) {
  // ROS 2 통신 컨텍스트를 만든 뒤 spin이 타이머 콜백을 처리한다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<GoalSource>());
  rclcpp::shutdown();
  return 0;
}
