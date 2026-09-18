#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"
#include "trajectory_msgs/msg/joint_trajectory.hpp"
#include "trajectory_msgs/msg/joint_trajectory_point.hpp"

namespace {
constexpr std::size_t kSamples = 101;
constexpr double kVmax = 0.7;   // rad/s
constexpr double kAmax = 1.0;   // rad/s²
constexpr double kJmax = 1.5;   // rad/s³
constexpr double kPi = 3.14159265358979323846;

struct Sample { double q, v, a; };

// 고정 크기 배열만 쓰는 수학 경로: 계획 콜백의 산술 횟수는 목표 거리와 무관하게 101회다.
std::array<Sample, kSamples> make_samples(double start, double goal, double duration) {
  std::array<Sample, kSamples> samples{};
  const double delta = goal - start;
  for (std::size_t i = 0; i < kSamples; ++i) {
    const double s = static_cast<double>(i) / static_cast<double>(kSamples - 1);
    // 5차 smoothstep: q=q0+Δ(10s³-15s⁴+6s⁵), s=t/T.
    const double s2 = s * s, s3 = s2 * s;
    const double blend = 10.0 * s3 - 15.0 * s3 * s + 6.0 * s3 * s2;
    // q의 시간 미분. T로 나눌 때마다 물리 단위가 rad/s, rad/s²로 바뀐다.
    const double velocity = delta * 30.0 * s2 * (1.0 - s) * (1.0 - s) / duration;
    const double acceleration = delta * 60.0 * s * (1.0 - s) * (1.0 - 2.0 * s)
                                / (duration * duration);
    samples[i] = {start + delta * blend, velocity, acceleration};
  }
  return samples;
}
}  // namespace

// goal_rad를 JointTrajectory로 바꾼다. 생성만 하며 하드웨어 구동은 하지 않는다.
class JerkLimitedPlanner final : public rclcpp::Node {
 public:
  JerkLimitedPlanner() : Node("jerk_limited_planner") {
    // 요청과 결과 모두 RELIABLE, 깊이 1: 이전 계획이 쌓이지 않게 한다.
    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
    publisher_ = create_publisher<trajectory_msgs::msg::JointTrajectory>("/study/trajectory", qos);
    subscription_ = create_subscription<std_msgs::msg::Float64>(
        "/study/goal_rad", qos,
        [this](std_msgs::msg::Float64::ConstSharedPtr msg) { on_goal(msg->data); });
  }

 private:
  // 입력을 검사하고 해석적 최대치로 T를 늘려 v/a/jerk의 연속시간 상한을 지킨다.
  void on_goal(double goal) {
    if (!std::isfinite(goal) || std::abs(goal) > kPi) {
      RCLCPP_WARN(get_logger(), "nonfinite/out-of-range goal rejected");
      return;
    }
    const double distance = std::abs(goal - start_);
    // max|v|=15/8·|Δ|/T, max|a|=10/√3·|Δ|/T², max|j|=60·|Δ|/T³.
    // 세 조건을 각각 T에 대해 풀고 최대를 취하면 모든 t에서 경계를 지킨다.
    const double duration = std::max({0.5, 1.875 * distance / kVmax,
        std::sqrt((10.0 / std::sqrt(3.0)) * distance / kAmax),
        std::cbrt(60.0 * distance / kJmax)});
    const auto samples = make_samples(start_, goal, duration);

    trajectory_msgs::msg::JointTrajectory trajectory;
    // Header.stamp=계획 생성 ROS 시각. time_from_start는 이 시작점 이후의 상대 Duration.
    trajectory.header.stamp = now();
    trajectory.joint_names.push_back("joint1");
    // ROS 메시지 벡터/DDS publish는 동적 할당 가능: 고정 배열 산술 ≠ 전체 노드의 hard RT.
    trajectory.points.reserve(kSamples);
    for (std::size_t i = 0; i < kSamples; ++i) {
      trajectory_msgs::msg::JointTrajectoryPoint point;
      point.positions = {samples[i].q};
      point.velocities = {samples[i].v};
      point.accelerations = {samples[i].a};
      // builtin_interfaces/Duration은 sec와 1e9분의 1초 nanosec로 수동 분리한다.
      // lround는 0.5 ns 경계의 누적 절단 오차를 피한다.
      const auto ns = static_cast<std::int64_t>(std::llround(
          duration * static_cast<double>(i) / static_cast<double>(kSamples - 1) * 1e9));
      point.time_from_start.sec = static_cast<std::int32_t>(ns / 1000000000LL);
      point.time_from_start.nanosec = static_cast<std::uint32_t>(ns % 1000000000LL);
      trajectory.points.push_back(std::move(point));
    }
    publisher_->publish(trajectory);
    RCLCPP_INFO(get_logger(), "start=%.3f goal=%.3f T=%.4f samples=%zu",
                start_, goal, duration, kSamples);
    // 데모는 실제 상태 피드백이 없으므로 이전 목표에 도달했다고 가정한다.
    // 생산 시스템에서는 JointState/컨트롤러 상태로 시작 위치를 갱신해야 한다.
    start_ = goal;
  }

  double start_{0.0};
  rclcpp::Publisher<trajectory_msgs::msg::JointTrajectory>::SharedPtr publisher_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr subscription_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  // 단일 executor가 목표 콜백을 직렬화해 start_ 공유 상태의 경합을 피한다.
  rclcpp::spin(std::make_shared<JerkLimitedPlanner>());
  rclcpp::shutdown();
  return 0;
}
