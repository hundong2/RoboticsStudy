#include <algorithm>
#include <chrono>
#include <memory>

#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

/** /cmd_vel의 선속도를 100 Hz로 적분해 /wheel/odom 위치를 만드는 1차원 모의 이동 베이스다. */
class MockMobileBase : public rclcpp::Node
{
public:
  MockMobileBase()
  : Node("mock_mobile_base")
  {
    // 속도 명령은 최신 값 하나만 유지하며 Action 서버와 같은 reliable QoS를 사용해야 연결된다.
    cmd_subscription_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", rclcpp::QoS(1).reliable(),
      [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
        // 비정상적으로 큰 명령이 모의 위치를 폭주시킬 수 있어 ±1 m/s로 제한한다.
        commanded_speed_mps_ = std::clamp(msg->linear.x, -1.0, 1.0);
      });

    odom_publisher_ = this->create_publisher<nav_msgs::msg::Odometry>(
      "/wheel/odom", rclcpp::QoS(1).reliable());
    integration_timer_ = this->create_wall_timer(10ms, std::bind(&MockMobileBase::integrate_and_publish, this));
  }

private:
  /** x[k+1] = x[k] + v[k]Δt 오일러 적분으로 위치를 갱신하고 Odometry를 게시한다. */
  void integrate_and_publish()
  {
    constexpr double dt_seconds = 0.01;
    position_x_m_ += commanded_speed_mps_ * dt_seconds;

    nav_msgs::msg::Odometry odometry;
    // now()는 ROS clock timestamp를 만들며 센서/상태 메시지를 시간축에서 정렬할 때 사용한다.
    odometry.header.stamp = this->now();
    odometry.header.frame_id = "odom";
    odometry.child_frame_id = "base_link";
    odometry.pose.pose.position.x = position_x_m_;
    // 단위 쿼터니언 (w=1)은 회전이 없음을 뜻한다.
    odometry.pose.pose.orientation.w = 1.0;
    odometry.twist.twist.linear.x = commanded_speed_mps_;
    odom_publisher_->publish(odometry);
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_publisher_;
  rclcpp::TimerBase::SharedPtr integration_timer_;
  double commanded_speed_mps_{0.0};
  double position_x_m_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // spin이 Subscription과 10 ms Timer 콜백을 순차 실행하므로 이 예제의 double에는 추가 mutex가 필요 없다.
  rclcpp::spin(std::make_shared<MockMobileBase>());
  rclcpp::shutdown();
  return 0;
}
