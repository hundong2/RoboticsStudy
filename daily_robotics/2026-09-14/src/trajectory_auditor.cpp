#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

/// 구현 내부를 신뢰하지 않고 topic 출력만으로 경로·필터·deadline 복구 계약을 검사하는 독립 노드다.
class TrajectoryAuditor final : public rclcpp::Node
{
public:
  TrajectoryAuditor()
  : Node("trajectory_auditor"), started_at_(std::chrono::steady_clock::now())
  {
    path_subscription_ = create_subscription<nav_msgs::msg::Path>(
      "/planning/local_trajectory", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
      [this](const nav_msgs::msg::Path & path) {audit_path(path);});
    optimizer_subscription_ = create_subscription<std_msgs::msg::String>(
      "/planning/optimizer_diagnostics",
      rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local(),
      [this](const std_msgs::msg::String & diagnostic) {
        saw_cft_ = diagnostic.data.find("cft=true") != std::string::npos;
        last_optimizer_diagnostic_ = diagnostic.data;
      });
    stop_subscription_ = create_subscription<std_msgs::msg::Bool>(
      "/safety/stop", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local(),
      [this](const std_msgs::msg::Bool & stop) {
        saw_stop_ = saw_stop_ || stop.data;
        if (saw_stop_ && !stop.data) {
          saw_recovery_ = true;
        }
      });
    status_subscription_ = create_subscription<std_msgs::msg::String>(
      "/nav/bt_status", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local(),
      [this](const std_msgs::msg::String & status) {
        saw_bt_recovery_ = saw_bt_recovery_ || status.data.find("RecoveryNode") != std::string::npos;
        saw_bt_follow_ = saw_bt_follow_ || status.data.find("FollowPath RUNNING") != std::string::npos;
      });
    command_subscription_ = create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
      [this](const geometry_msgs::msg::Twist & command) {
        saw_moving_command_ = saw_moving_command_ || command.linear.x > 0.05;
        if (saw_stop_ && std::abs(command.linear.x) < 1.0e-9 && std::abs(command.angular.z) < 1.0e-9) {
          saw_zero_command_ = true;
        }
      });

    audit_publisher_ = create_publisher<std_msgs::msg::String>(
      "/nav/audit", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
    timer_ = create_wall_timer(500ms, [this]() {publish_verdict();});
  }

private:
  /// Path의 유한성, endpoint 보존, knot 간격, timestamp 단조 증가를 독립 계산한다.
  void audit_path(const nav_msgs::msg::Path & path)
  {
    ++path_samples_;
    path_valid_ = path.poses.size() == 17U;
    if (!path_valid_) {
      return;
    }
    const auto & first = path.poses.front();
    const auto & last = path.poses.back();
    path_valid_ = std::hypot(first.pose.position.x, first.pose.position.y) < 1.0e-6 &&
      std::hypot(last.pose.position.x - 4.0, last.pose.position.y) < 1.0e-6;

    double previous_stamp = rclcpp::Time(first.header.stamp).seconds();
    max_segment_m_ = 0.0;
    for (std::size_t i = 1; i < path.poses.size(); ++i) {
      const auto & a = path.poses[i - 1U].pose.position;
      const auto & b = path.poses[i].pose.position;
      path_valid_ = path_valid_ && std::isfinite(b.x) && std::isfinite(b.y);
      max_segment_m_ = std::max(max_segment_m_, std::hypot(b.x - a.x, b.y - a.y));
      const double stamp = rclcpp::Time(path.poses[i].header.stamp).seconds();
      path_valid_ = path_valid_ && stamp > previous_stamp;
      previous_stamp = stamp;
    }
    path_valid_ = path_valid_ && max_segment_m_ < 0.45;
  }

  /// 8초 이후 의도적 heartbeat 단절과 복구까지 모두 관찰했을 때만 PASS를 latch한다.
  void publish_verdict()
  {
    const double elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started_at_).count();
    const bool passed = elapsed > 8.0 && path_valid_ && saw_cft_ && saw_stop_ && saw_recovery_ &&
      saw_bt_recovery_ && saw_bt_follow_ && saw_moving_command_ && saw_zero_command_;

    std_msgs::msg::String verdict;
    std::ostringstream text;
    text << (passed ? "PASS" : "WAIT")
         << " paths=" << path_samples_ << " path_valid=" << path_valid_
         << " max_segment_m=" << max_segment_m_ << " cft=" << saw_cft_
         << " deadline_stop=" << saw_stop_ << " deadline_recovery=" << saw_recovery_
         << " bt_recovery=" << saw_bt_recovery_ << " bt_follow=" << saw_bt_follow_
         << " moving_cmd=" << saw_moving_command_ << " zero_cmd=" << saw_zero_command_;
    verdict.data = text.str();
    audit_publisher_->publish(verdict);
  }

  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr path_subscription_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr optimizer_subscription_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_subscription_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr command_subscription_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::chrono::steady_clock::time_point started_at_;
  std::string last_optimizer_diagnostic_;
  std::uint64_t path_samples_{0U};
  double max_segment_m_{0.0};
  bool path_valid_{false};
  bool saw_cft_{false};
  bool saw_stop_{false};
  bool saw_recovery_{false};
  bool saw_bt_recovery_{false};
  bool saw_bt_follow_{false};
  bool saw_moving_command_{false};
  bool saw_zero_command_{false};
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<daily_robotics::TrajectoryAuditor>());
  rclcpp::shutdown();
  return 0;
}
