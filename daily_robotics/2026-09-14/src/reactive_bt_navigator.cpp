#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

/// Nav2의 PipelineSequence/RecoveryNode 의미를 작은 상태기로 재현해 BT tick을 읽기 쉽게 만든 노드다.
class ReactiveBtNavigator final : public rclcpp::Node
{
public:
  ReactiveBtNavigator()
  : Node("reactive_bt_navigator")
  {
    trajectory_subscription_ = create_subscription<nav_msgs::msg::Path>(
      "/planning/local_trajectory", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
      [this](const nav_msgs::msg::Path & path) {
        trajectory_ = path;
        has_trajectory_ = path.poses.size() >= 3U;
        ++trajectory_generation_;
      });
    stop_subscription_ = create_subscription<std_msgs::msg::Bool>(
      "/safety/stop", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local(),
      [this](const std_msgs::msg::Bool & stop) {safety_stop_ = stop.data;});

    raw_command_publisher_ = create_publisher<geometry_msgs::msg::Twist>(
      "/cmd_vel_raw", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());
    status_publisher_ = create_publisher<std_msgs::msg::String>(
      "/nav/bt_status", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());

    // Nav2 bt_loop_duration과 같은 개념으로 50 ms마다 루트부터 반응형 조건을 다시 평가한다.
    timer_ = create_wall_timer(50ms, [this]() {tick_tree();});
  }

private:
  enum class State {kWaitingForPath, kFollowing, kRecovery};

  /// 안전 조건→경로 조건→추종 순서로 매 tick 재평가하고 raw 속도 명령을 만든다.
  void tick_tree()
  {
    State next_state = State::kWaitingForPath;
    geometry_msgs::msg::Twist command;

    if (safety_stop_) {
      // RecoveryNode 진입: planner deadline 실패 중에는 하위 FollowPath 결과와 무관하게 0 속도를 낸다.
      next_state = State::kRecovery;
    } else if (has_trajectory_) {
      next_state = State::kFollowing;
      const auto & lookahead = trajectory_.poses[2U].pose.position;
      const double heading_error = std::atan2(lookahead.y, std::max(lookahead.x, 1.0e-6));
      // v = min(v_max, 2||p_lookahead||), w = clamp(1.5*heading_error): 단순 추종 제어식이다.
      command.linear.x = std::min(0.40, 2.0 * std::hypot(lookahead.x, lookahead.y));
      command.angular.z = std::clamp(1.5 * heading_error, -0.8, 0.8);
    }
    raw_command_publisher_->publish(command);

    ++ticks_;
    if (next_state != state_ || (ticks_ % 20U) == 0U) {
      state_ = next_state;
      publish_status();
    }
  }

  /// BT 노드 이름을 상태 문자열에 남겨 XML 계약과 실행 로그를 함께 읽을 수 있게 한다.
  void publish_status()
  {
    std_msgs::msg::String status;
    switch (state_) {
      case State::kWaitingForPath:
        status.data = "PipelineSequence: ComputePath RUNNING; FollowPath IDLE";
        break;
      case State::kFollowing:
        status.data = "PipelineSequence: ComputePath SUCCESS; FollowPath RUNNING; generation=" +
          std::to_string(trajectory_generation_);
        break;
      case State::kRecovery:
        status.data = "RecoveryNode: planner deadline missed -> StopRobot RUNNING";
        break;
    }
    status_publisher_->publish(status);
  }

  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr trajectory_subscription_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_subscription_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr raw_command_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  nav_msgs::msg::Path trajectory_;
  State state_{State::kWaitingForPath};
  std::uint64_t ticks_{0U};
  std::uint64_t trajectory_generation_{0U};
  bool has_trajectory_{false};
  bool safety_stop_{true};
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<daily_robotics::ReactiveBtNavigator>());
  rclcpp::shutdown();
  return 0;
}
