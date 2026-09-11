#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>

#include "message_filters/subscriber.h"
#include "message_filters/synchronizer.h"
#include "message_filters/sync_policies/exact_time.h"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace daily_robotics
{

// quaternion의 평면 yaw를 atan2 공식으로 복원한다.
double quaternion_to_yaw(const geometry_msgs::msg::Quaternion & q)
{
  return std::atan2(
    2.0 * (q.w * q.z + q.x * q.y),
    1.0 - 2.0 * (q.y * q.y + q.z * q.z));
}

double wrap_angle(double angle)
{
  constexpr double kPi = 3.14159265358979323846;
  while (angle > kPi) {
    angle -= 2.0 * kPi;
  }
  while (angle < -kPi) {
    angle += 2.0 * kPi;
  }
  return angle;
}

// Estimator와 독립된 이 노드는 같은 stamp의 ground truth/VO pose를 비교해 회귀를 감지한다.
class VoAuditor final : public rclcpp::Node
{
public:
  using Odometry = nav_msgs::msg::Odometry;
  // Simulator와 estimator가 같은 LiDAR stamp를 전달하므로 audit에는 ExactTime이 맞다.
  using SyncPolicy = message_filters::sync_policies::ExactTime<Odometry, Odometry>;

  VoAuditor()
  : Node("vo_auditor")
  {
    // Transient Local은 늦게 실행한 `ros2 topic echo --once`도 최신 판정을 받게 한다.
    audit_pub_ = create_publisher<std_msgs::msg::String>(
      "/vo/audit", rclcpp::QoS(1).reliable().transient_local());

    const auto qos = rclcpp::QoS(20).reliable();
    // 최신 값 하나만 저장하면 빠른 truth가 느린 estimate를 덮어쓸 수 있다.
    // ExactTime Synchronizer의 유한 queue 20이 동일 stamp를 보존하고 도착 순서와 무관하게 짝짓는다.
    truth_sub_.subscribe(this, "/sim/ground_truth", qos.get_rmw_qos_profile());
    estimate_sub_.subscribe(this, "/vo/odometry", qos.get_rmw_qos_profile());
    sync_ = std::make_shared<message_filters::Synchronizer<SyncPolicy>>(
      SyncPolicy(20U), truth_sub_, estimate_sub_);
    sync_->registerCallback(
      std::bind(&VoAuditor::on_pair, this, std::placeholders::_1, std::placeholders::_2));
  }

private:
  // ExactTime이 같은 Header stamp의 두 Odometry를 전달한 때만 회귀 오차를 계산한다.
  void on_pair(const Odometry::ConstSharedPtr & truth, const Odometry::ConstSharedPtr & estimate)
  {
    const double dx = truth->pose.pose.position.x - estimate->pose.pose.position.x;
    const double dy = truth->pose.pose.position.y - estimate->pose.pose.position.y;
    const double position_error = std::hypot(dx, dy);
    const double truth_yaw = quaternion_to_yaw(truth->pose.pose.orientation);
    const double estimate_yaw = quaternion_to_yaw(estimate->pose.pose.orientation);
    const double yaw_error = std::abs(wrap_angle(truth_yaw - estimate_yaw));

    ++comparison_count_;
    max_position_error_ = std::max(max_position_error_, position_error);
    max_yaw_error_ = std::max(max_yaw_error_, yaw_error);

    // 초기 20 frame 뒤부터 5 cm/0.03 rad 계약을 판정한다. 순간 PASS가 아니라 누적 max를 본다.
    const bool enough_samples = comparison_count_ >= 20U;
    const bool pass = enough_samples && max_position_error_ < 0.05 && max_yaw_error_ < 0.03;

    if (comparison_count_ % 20U == 0U) {
      std_msgs::msg::String message;
      std::ostringstream stream;
      stream << std::fixed << std::setprecision(5)
             << (pass ? "PASS" : "WARMUP_OR_FAIL")
             << " samples=" << comparison_count_
             << " position_error_m=" << position_error
             << " yaw_error_rad=" << yaw_error
             << " max_position_error_m=" << max_position_error_
             << " max_yaw_error_rad=" << max_yaw_error_;
      message.data = stream.str();
      audit_pub_->publish(message);
    }
  }

  std::uint64_t comparison_count_{0U};
  double max_position_error_{0.0};
  double max_yaw_error_{0.0};
  message_filters::Subscriber<Odometry> truth_sub_;
  message_filters::Subscriber<Odometry> estimate_sub_;
  std::shared_ptr<message_filters::Synchronizer<SyncPolicy>> sync_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_pub_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  const auto node = std::make_shared<daily_robotics::VoAuditor>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
