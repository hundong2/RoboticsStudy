#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"

using namespace std::chrono_literals;

namespace daily_robotics_2026_09_07
{

// 이 노드는 planner와 actuator 사이의 독립 안전 경계다. DWA가 잘못된 값을 내거나
// 멈추더라도 속도·가속도 clamp와 300 ms watchdog으로 최종 /cmd_vel을 제한한다.
class CommandGuardComponent final : public rclcpp::Node
{
public:
  explicit CommandGuardComponent(const rclcpp::NodeOptions & options)
  : Node("command_guard", options)
  {
    // KeepLast(1)은 과거 명령을 쌓지 않고 최신 제어 의도만 유지한다.
    raw_command_subscription_ = create_subscription<geometry_msgs::msg::TwistStamped>(
      "/cmd_vel_raw", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
      std::bind(&CommandGuardComponent::on_raw_command, this, std::placeholders::_1));
    safe_command_publisher_ = create_publisher<geometry_msgs::msg::Twist>(
      "/cmd_vel", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());

    // watchdog는 planner의 10 Hz 주기보다 빠른 20 Hz로 검사해 300 ms stale을 놓치지 않는다.
    watchdog_timer_ = create_wall_timer(
      50ms, std::bind(&CommandGuardComponent::on_watchdog, this));

    RCLCPP_INFO(
      get_logger(),
      "CommandGuard ready: |v|<=%.2f |w|<=%.2f timeout=%d ms intra-process=%s",
      kMaximumLinearVelocity, kMaximumAngularVelocity, kCommandTimeoutMilliseconds,
      options.use_intra_process_comms() ? "true" : "false");
  }

private:
  static constexpr double kMaximumLinearVelocity = 0.75;
  static constexpr double kMaximumAngularVelocity = 1.00;
  static constexpr double kMaximumLinearAcceleration = 0.60;
  static constexpr double kMaximumAngularAcceleration = 1.50;
  static constexpr int kCommandTimeoutMilliseconds = 300;

  // DWA 명령 하나를 받아 절대 속도와 한 주기 변화량을 모두 제한한다.
  void on_raw_command(geometry_msgs::msg::TwistStamped::UniquePtr raw_command)
  {
    const void * const received_address = raw_command.get();
    const auto now_steady = std::chrono::steady_clock::now();
    double delta_seconds = 0.10;
    if (received_any_command_) {
      delta_seconds = std::chrono::duration<double>(now_steady - last_command_time_).count();
      // 긴 정지 뒤 큰 변화량을 허용하지 않도록 가속도 계산 dt를 정상 범위로 제한한다.
      delta_seconds = std::clamp(delta_seconds, 0.01, 0.20);
    }

    const double requested_linear = std::clamp(
      raw_command->twist.linear.x,
      -kMaximumLinearVelocity, kMaximumLinearVelocity);
    const double requested_angular = std::clamp(
      raw_command->twist.angular.z,
      -kMaximumAngularVelocity, kMaximumAngularVelocity);

    // |v_k-v_(k-1)| <= a_max*dt를 코드로 옮긴 rate limiter다.
    const double maximum_linear_delta = kMaximumLinearAcceleration * delta_seconds;
    const double maximum_angular_delta = kMaximumAngularAcceleration * delta_seconds;
    last_linear_velocity_ += std::clamp(
      requested_linear - last_linear_velocity_,
      -maximum_linear_delta, maximum_linear_delta);
    last_angular_velocity_ += std::clamp(
      requested_angular - last_angular_velocity_,
      -maximum_angular_delta, maximum_angular_delta);

    publish_safe_command(last_linear_velocity_, last_angular_velocity_);
    last_command_time_ = now_steady;
    received_any_command_ = true;
    watchdog_stopped_ = false;

    ++command_count_;
    if (command_count_ % 10U == 1U) {
      // planner의 publish 직전 주소와 같으면 UniquePtr 소유권이 복사 없이 넘어온 것이다.
      RCLCPP_INFO(
        get_logger(),
        "raw unique_ptr=%p requested(v=%.3f,w=%.3f) safe(v=%.3f,w=%.3f)",
        received_address, raw_command->twist.linear.x, raw_command->twist.angular.z,
        last_linear_velocity_, last_angular_velocity_);
    }
  }

  // planner 명령이 300 ms 동안 없으면 정지 명령을 한 번 게시한다.
  void on_watchdog()
  {
    if (!received_any_command_ || watchdog_stopped_) {
      return;
    }

    const auto stale_for = std::chrono::steady_clock::now() - last_command_time_;
    if (stale_for <= std::chrono::milliseconds(kCommandTimeoutMilliseconds)) {
      return;
    }

    last_linear_velocity_ = 0.0;
    last_angular_velocity_ = 0.0;
    publish_safe_command(0.0, 0.0);
    watchdog_stopped_ = true;
    RCLCPP_ERROR(
      get_logger(), "planner command stale for >%d ms: published stop",
      kCommandTimeoutMilliseconds);
  }

  void publish_safe_command(const double linear_velocity, const double angular_velocity)
  {
    // 최종 actuator consumer도 같은 프로세스라면 이 UniquePtr 경로를 그대로 활용할 수 있다.
    auto safe_command = std::make_unique<geometry_msgs::msg::Twist>();
    safe_command->linear.x = linear_velocity;
    safe_command->angular.z = angular_velocity;
    safe_command_publisher_->publish(std::move(safe_command));
  }

  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr
    raw_command_subscription_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr safe_command_publisher_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;

  std::chrono::steady_clock::time_point last_command_time_{};
  double last_linear_velocity_{0.0};
  double last_angular_velocity_{0.0};
  bool received_any_command_{false};
  bool watchdog_stopped_{false};
  std::uint64_t command_count_{0U};
};

}  // namespace daily_robotics_2026_09_07

RCLCPP_COMPONENTS_REGISTER_NODE(
  daily_robotics_2026_09_07::CommandGuardComponent)
