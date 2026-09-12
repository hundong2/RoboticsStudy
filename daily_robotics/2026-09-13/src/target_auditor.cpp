#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "geometry_msgs/msg/point.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace daily_robotics_2026_09_13
{

// 목표 생성과 독립 FK 검증을 한 노드에 묶어 실습이 자동으로 움직이고 PASS/WAIT를 스스로 판정하게 한다.
class TargetAuditor final : public rclcpp::Node
{
public:
  TargetAuditor()
  : Node("target_auditor")
  {
    // KeepLast(1)+reliable은 느린 구독자에게 오래된 목표를 쌓지 않고 최신 목표만 보존한다.
    target_publisher_ = create_publisher<geometry_msgs::msg::Point>(
      "/arm/target_xy", rclcpp::QoS(1).reliable());
    audit_publisher_ = create_publisher<std_msgs::msg::String>(
      "/arm/audit", rclcpp::QoS(1).reliable());

    joint_subscription_ = create_subscription<sensor_msgs::msg::JointState>(
      "/joint_states", rclcpp::QoS(10).reliable(),
      [this](const sensor_msgs::msg::JointState::SharedPtr message) { receive_joints(*message); });
    status_subscription_ = create_subscription<std_msgs::msg::Float64MultiArray>(
      "/ik_controller/status", rclcpp::QoS(10).reliable(),
      [this](const std_msgs::msg::Float64MultiArray::SharedPtr message) {
        if (message->data.size() == 8U) {
          controller_error_m_ = message->data[2];
          damping_ = message->data[3];
          publish_misses_ = message->data[6];
          interface_misses_ = message->data[7];
          ++status_samples_;
        }
      });

    // 20 ms timer는 목표를 반복 발행해 late-joining controller도 현재 목표를 받게 한다.
    target_timer_ = create_wall_timer(20ms, [this]() { publish_target(); });
    audit_timer_ = create_wall_timer(1s, [this]() { publish_audit(); });
    target_started_ = now();
  }

private:
  void publish_target()
  {
    static constexpr std::array<std::array<double, 2>, 4> kTargets{{
      {{1.20, 0.20}}, {{0.80, 0.85}}, {{1.30, -0.35}}, {{1.797, 0.0}}
    }};
    const double elapsed = (now() - target_started_).seconds();
    const std::size_t next_index = static_cast<std::size_t>(elapsed / 4.0) % kTargets.size();
    if (next_index != target_index_) {
      target_index_ = next_index;
      target_changed_ = now();
    }

    geometry_msgs::msg::Point message;
    message.x = kTargets[target_index_][0];
    message.y = kTargets[target_index_][1];
    message.z = 0.0;
    target_x_ = message.x;
    target_y_ = message.y;
    target_publisher_->publish(message);
  }

  void receive_joints(const sensor_msgs::msg::JointState & message)
  {
    // JointState 순서는 설정에 따라 달라질 수 있으므로 name으로 position을 찾는다.
    for (std::size_t i = 0; i < message.name.size() && i < message.position.size(); ++i) {
      if (message.name[i] == "shoulder_joint") {
        q1_ = message.position[i];
        have_q1_ = true;
      } else if (message.name[i] == "elbow_joint") {
        q2_ = message.position[i];
        have_q2_ = true;
      }
    }
    ++joint_samples_;
  }

  void publish_audit()
  {
    std_msgs::msg::String message;
    std::ostringstream stream;
    if (!have_q1_ || !have_q2_ || status_samples_ == 0U) {
      stream << "WAIT samples(joint/status)=" << joint_samples_ << "/" << status_samples_;
    } else {
      // Controller와 독립적으로 같은 2R 순기구학을 다시 계산해 topic wiring 오류까지 검출한다.
      const double x = std::cos(q1_) + 0.8 * std::cos(q1_ + q2_);
      const double y = std::sin(q1_) + 0.8 * std::sin(q1_ + q2_);
      const double independent_error = std::hypot(target_x_ - x, target_y_ - y);
      const double settled_seconds = (now() - target_changed_).seconds();
      const bool pass = settled_seconds >= 1.5 && independent_error < 0.03 &&
        controller_error_m_ < 0.03 && interface_misses_ == 0.0;
      stream << (pass ? "PASS" : "WAIT")
             << " target=(" << target_x_ << "," << target_y_ << ")"
             << " independent_error_m=" << independent_error
             << " controller_error_m=" << controller_error_m_
             << " lambda=" << damping_
             << " rt_publish_misses=" << publish_misses_
             << " interface_misses=" << interface_misses_
             << " samples(joint/status)=" << joint_samples_ << "/" << status_samples_;
    }
    message.data = stream.str();
    audit_publisher_->publish(message);
  }

  rclcpp::Publisher<geometry_msgs::msg::Point>::SharedPtr target_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_publisher_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_subscription_;
  rclcpp::Subscription<std_msgs::msg::Float64MultiArray>::SharedPtr status_subscription_;
  rclcpp::TimerBase::SharedPtr target_timer_;
  rclcpp::TimerBase::SharedPtr audit_timer_;

  rclcpp::Time target_started_;
  rclcpp::Time target_changed_{0, 0, RCL_ROS_TIME};
  std::size_t target_index_{0U};
  std::size_t joint_samples_{0U};
  std::size_t status_samples_{0U};
  double target_x_{1.20};
  double target_y_{0.20};
  double q1_{0.0};
  double q2_{0.0};
  double controller_error_m_{std::numeric_limits<double>::infinity()};
  double damping_{0.0};
  double publish_misses_{0.0};
  double interface_misses_{0.0};
  bool have_q1_{false};
  bool have_q2_{false};
};

}  // namespace daily_robotics_2026_09_13

int main(int argc, char ** argv)
{
  // init은 DDS/RMW와 ROS argument를 초기화하고, spin은 timer/subscription callback을 실행한다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<daily_robotics_2026_09_13::TargetAuditor>());
  rclcpp::shutdown();
  return 0;
}
