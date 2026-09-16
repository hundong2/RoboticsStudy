#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/u_int32_multi_array.hpp"

// 적분기와 독립된 해석식을 써서 동일한 구현 오류를 두 노드가 공유하지 않도록 한다.
class IntegrationAuditor final : public rclcpp::Node {
public:
  IntegrationAuditor() : Node("integration_auditor") {
    delta_sub_ = create_subscription<nav_msgs::msg::Odometry>(
        "/imu/delta", rclcpp::QoS(10),
        [this](nav_msgs::msg::Odometry::ConstSharedPtr msg) { check(*msg); });
    stats_sub_ = create_subscription<std_msgs::msg::UInt32MultiArray>(
        "/imu/integration_stats", rclcpp::QoS(1).reliable().transient_local(),
        [this](std_msgs::msg::UInt32MultiArray::ConstSharedPtr msg) {
          if (msg->data.size() >= 4) {
            resets_ = msg->data[0];
            overflows_ = msg->data[1];
            max_batch_ = msg->data[3];
          }
        });
    audit_pub_ = create_publisher<std_msgs::msg::String>(
        "/imu/audit", rclcpp::QoS(1).reliable().transient_local());
  }

private:
  // 상수 body-x 가속도 a, yaw rate w의 정확한 원점 상대 해:
  // x=a/w²(1-cos wt), y=a/w²(wt-sin wt). IMU integration 구현과 수학적으로 비교한다.
  void check(const nav_msgs::msg::Odometry &odom) {
    const auto stamp_ns = rclcpp::Time(odom.header.stamp, RCL_ROS_TIME).nanoseconds();
    constexpr std::int64_t kStartNs = 10'000'000'000LL;
    const double t = static_cast<double>(stamp_ns - kStartNs) * 1e-9;
    if (t < 0.0 || t >= 4.0) {
      return;
    }
    constexpr double a = 0.5;
    constexpr double w = 0.3;
    const double expected_x = a / (w * w) * (1.0 - std::cos(w * t));
    const double expected_y = a / (w * w) * (w * t - std::sin(w * t));
    const auto &q = odom.pose.pose.orientation;
    const double yaw = 2.0 * std::atan2(q.z, q.w);
    const double error = std::hypot(odom.pose.pose.position.x - expected_x,
                                    odom.pose.pose.position.y - expected_y);
    // 두 번째 재생 주기의 2.5초 이후에만 PASS: 역행 복구까지 확인한다.
    if (t < 2.5 || resets_ == 0 || reported_) {
      return;
    }
    const bool passed = error < 0.02 && std::abs(yaw - w * t) < 0.01 &&
                        overflows_ == 0 && max_batch_ <= 16;
    std::ostringstream report;
    report << (passed ? "PASS" : "FAIL") << " t=" << t
           << " position_error_m=" << error << " yaw_error_rad=" << std::abs(yaw - w * t)
           << " clock_resets=" << resets_ << " queue_overflows=" << overflows_
           << " max_batch=" << max_batch_;
    std_msgs::msg::String result;
    result.data = report.str();
    audit_pub_->publish(result);
    RCLCPP_INFO(get_logger(), "%s", result.data.c_str());
    reported_ = true;
  }

  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr delta_sub_;
  rclcpp::Subscription<std_msgs::msg::UInt32MultiArray>::SharedPtr stats_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_pub_;
  std::uint32_t resets_{0}, overflows_{0}, max_batch_{0};
  bool reported_{false};
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<IntegrationAuditor>());
  rclcpp::shutdown();
  return 0;
}
