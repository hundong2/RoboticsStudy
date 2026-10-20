#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace
{
struct TruthPose
{
  std::array<double, 3> p{};
  std::array<double, 3> rpy{};
};

geometry_msgs::msg::Quaternion quaternion_from_rpy(const double roll, const double pitch, const double yaw)
{
  // ZYX Euler 각을 단위 쿼터니언으로 바꾼다. ROS의 Quaternion은 (x,y,z,w) 순서다.
  const double cr = std::cos(0.5 * roll);
  const double sr = std::sin(0.5 * roll);
  const double cp = std::cos(0.5 * pitch);
  const double sp = std::sin(0.5 * pitch);
  const double cy = std::cos(0.5 * yaw);
  const double sy = std::sin(0.5 * yaw);
  geometry_msgs::msg::Quaternion q;
  q.w = cr * cp * cy + sr * sp * sy;
  q.x = sr * cp * cy - cr * sp * sy;
  q.y = cr * sp * cy + sr * cp * sy;
  q.z = cr * cp * sy - sr * sp * cy;
  return q;
}

std::array<double, 3> world_to_previous_body(
  const std::array<double, 3> & delta_world, const std::array<double, 3> & previous_rpy)
{
  // p_rel = R_prev^T (p_current - p_prev): 상대 이동은 이전 카메라 좌표계에서 표현해야 한다.
  const double cr = std::cos(previous_rpy[0]);
  const double sr = std::sin(previous_rpy[0]);
  const double cp = std::cos(previous_rpy[1]);
  const double sp = std::sin(previous_rpy[1]);
  const double cy = std::cos(previous_rpy[2]);
  const double sy = std::sin(previous_rpy[2]);

  // R = Rz(yaw) Ry(pitch) Rx(roll)의 전치행렬을 직접 곱한다.
  return {
    cy * cp * delta_world[0] + sy * cp * delta_world[1] - sp * delta_world[2],
    (cy * sp * sr - sy * cr) * delta_world[0] +
    (sy * sp * sr + cy * cr) * delta_world[1] + cp * sr * delta_world[2],
    (cy * sp * cr + sy * sr) * delta_world[0] +
    (sy * sp * cr - cy * sr) * delta_world[1] + cp * cr * delta_world[2]};
}

TruthPose make_truth(const std::uint32_t sequence)
{
  const double k = static_cast<double>(sequence);
  TruthPose pose;
  pose.p = {0.055 * k, 0.45 * std::sin(0.055 * k), 0.18 * std::sin(0.031 * k)};
  pose.rpy = {
    0.045 * std::sin(0.041 * k),
    0.035 * std::cos(0.037 * k),
    0.011 * k};
  return pose;
}
}  // namespace

class PoseFactorSimulator : public rclcpp::Node
{
public:
  PoseFactorSimulator()
  : Node("pose_factor_simulator")
  {
    // reliable + depth 20: 학습용 factor를 의도적으로 유실하지 않고, 짧은 시작 지연을 흡수한다.
    const auto qos = rclcpp::QoS(rclcpp::KeepLast(20)).reliable();
    truth_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>("/sim/truth_pose", qos);
    relative_pub_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/sim/relative_pose", qos);
    absolute_pub_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/sim/absolute_pose", qos);

    // 10 Hz는 각 factor와 진실값을 눈으로 추적하기 쉬우면서 smoke test를 빠르게 끝내는 주기다.
    timer_ = create_wall_timer(100ms, std::bind(&PoseFactorSimulator::on_timer, this));
  }

private:
  struct PendingAbsolute
  {
    bool valid{false};
    std::uint32_t release_sequence{0U};
    geometry_msgs::msg::PoseWithCovarianceStamped message{};
  };

  geometry_msgs::msg::PoseWithCovarianceStamped make_absolute(
    const TruthPose & truth, const builtin_interfaces::msg::Time & stamp) const
  {
    geometry_msgs::msg::PoseWithCovarianceStamped message;
    // stamp는 콜백 도착시각이 아니라 센서가 이 자세를 측정한 시각이다.
    message.header.stamp = stamp;
    message.header.frame_id = "map";

    const double k = static_cast<double>(sequence_);
    message.pose.pose.position.x = truth.p[0] + 0.025 * std::sin(0.37 * k);
    message.pose.pose.position.y = truth.p[1] + 0.020 * std::cos(0.29 * k);
    message.pose.pose.position.z = truth.p[2] + 0.015 * std::sin(0.23 * k);
    message.pose.pose.orientation = quaternion_from_rpy(
      truth.rpy[0] + 0.008 * std::sin(0.17 * k),
      truth.rpy[1] + 0.008 * std::cos(0.19 * k),
      truth.rpy[2] + 0.012 * std::sin(0.13 * k));

    // PoseWithCovariance의 36개 값은 [x,y,z,roll,pitch,yaw] 6x6 행렬의 row-major 순서다.
    message.pose.covariance.fill(0.0);
    message.pose.covariance[0] = 0.025 * 0.025;
    message.pose.covariance[7] = 0.025 * 0.025;
    message.pose.covariance[14] = 0.020 * 0.020;
    message.pose.covariance[21] = 0.015 * 0.015;
    message.pose.covariance[28] = 0.015 * 0.015;
    message.pose.covariance[35] = 0.020 * 0.020;
    return message;
  }

  void publish_relative(
    const TruthPose & previous, const TruthPose & current,
    const builtin_interfaces::msg::Time & stamp)
  {
    geometry_msgs::msg::PoseWithCovarianceStamped message;
    message.header.stamp = stamp;
    // 상대 자세 T_prev_current가 이전 body 프레임에서 표현됐다는 명시적 계약이다.
    message.header.frame_id = "base_previous";

    const std::array<double, 3> delta_world{
      current.p[0] - previous.p[0], current.p[1] - previous.p[1], current.p[2] - previous.p[2]};
    auto delta_body = world_to_previous_body(delta_world, previous.rpy);
    const double k = static_cast<double>(sequence_);
    delta_body[0] += 0.0020 * std::sin(0.41 * k);
    delta_body[1] += 0.0015 * std::cos(0.33 * k);
    delta_body[2] += 0.0010 * std::sin(0.27 * k);
    message.pose.pose.position.x = delta_body[0];
    message.pose.pose.position.y = delta_body[1];
    message.pose.pose.position.z = delta_body[2];

    // 작은 각도 구간에서는 Δroll/Δpitch/Δyaw가 SO(3) log-map의 교육용 근사로 동작한다.
    message.pose.pose.orientation = quaternion_from_rpy(
      current.rpy[0] - previous.rpy[0] + 0.0008 * std::sin(0.31 * k),
      current.rpy[1] - previous.rpy[1] + 0.0008 * std::cos(0.35 * k),
      current.rpy[2] - previous.rpy[2] + 0.0010 * std::sin(0.21 * k));
    message.pose.covariance.fill(0.0);
    message.pose.covariance[0] = 0.003 * 0.003;
    message.pose.covariance[7] = 0.003 * 0.003;
    message.pose.covariance[14] = 0.002 * 0.002;
    message.pose.covariance[21] = 0.002 * 0.002;
    message.pose.covariance[28] = 0.002 * 0.002;
    message.pose.covariance[35] = 0.002 * 0.002;
    relative_pub_->publish(message);
  }

  void on_timer()
  {
    // Jazzy의 rclcpp::Time은 builtin_interfaces/Time으로 명시적으로 변환할 수 있다.
    const builtin_interfaces::msg::Time now = get_clock()->now();
    const TruthPose truth = make_truth(sequence_);

    geometry_msgs::msg::PoseStamped truth_message;
    truth_message.header.stamp = now;
    truth_message.header.frame_id = "map";
    truth_message.pose.position.x = truth.p[0];
    truth_message.pose.position.y = truth.p[1];
    truth_message.pose.position.z = truth.p[2];
    truth_message.pose.orientation = quaternion_from_rpy(
      truth.rpy[0], truth.rpy[1], truth.rpy[2]);
    truth_pub_->publish(truth_message);

    if (sequence_ > 0U) {
      publish_relative(previous_truth_, truth, now);
    }

    if (sequence_ == 0U) {
      // 첫 절대 factor는 즉시 보내 optimizer의 gauge freedom을 제거하는 prior로 사용한다.
      absolute_pub_->publish(make_absolute(truth, now));
    } else if ((sequence_ % 5U) == 0U) {
      // 이후 절대 factor는 세 tick 늦게 도착하지만 header.stamp는 측정시각을 보존한다.
      for (auto & pending : pending_absolute_) {
        if (!pending.valid) {
          pending.valid = true;
          pending.release_sequence = sequence_ + 3U;
          pending.message = make_absolute(truth, now);
          break;
        }
      }
    }

    for (auto & pending : pending_absolute_) {
      if (pending.valid && pending.release_sequence <= sequence_) {
        absolute_pub_->publish(pending.message);
        pending.valid = false;
      }
    }

    previous_truth_ = truth;
    ++sequence_;
  }

  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr truth_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr relative_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr absolute_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::array<PendingAbsolute, 4> pending_absolute_{};
  TruthPose previous_truth_{};
  std::uint32_t sequence_{0U};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // spin은 DDS 이벤트를 기다렸다가 timer callback을 실행하는 ROS 2 기본 실행 루프다.
  rclcpp::spin(std::make_shared<PoseFactorSimulator>());
  rclcpp::shutdown();
  return 0;
}
