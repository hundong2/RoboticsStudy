#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "trajectory_msgs/msg/joint_trajectory.hpp"

namespace {
constexpr double kVmax = 0.7, kAmax = 1.0, kJmax = 1.5;

// Duration을 초로 변환한다. nanosec 범위 검사는 호출자가 먼저 수행한다.
double seconds(const builtin_interfaces::msg::Duration &time) {
  return static_cast<double>(time.sec) + static_cast<double>(time.nanosec) * 1e-9;
}
}  // namespace

// 독립 구독자로서 메시지 구조와 이산 표본의 경계 및 미분 일관성을 검사한다.
class TrajectoryAuditor final : public rclcpp::Node {
 public:
  TrajectoryAuditor() : Node("trajectory_auditor") {
    auto input_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
    // 마지막 PASS/FAIL을 뒤늦게 시작한 smoke/ros2 topic echo도 읽게 TRANSIENT_LOCAL.
    auto result_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    publisher_ = create_publisher<std_msgs::msg::String>("/study/audit", result_qos);
    subscription_ = create_subscription<trajectory_msgs::msg::JointTrajectory>(
        "/study/trajectory", input_qos,
        [this](trajectory_msgs::msg::JointTrajectory::ConstSharedPtr msg) { audit(*msg); });
  }

 private:
  // 경계 위반이면 명시적 FAIL을 남긴다. 별개 프로세스이므로 계획 코드의 내부 상태를 신뢰하지 않는다.
  void audit(const trajectory_msgs::msg::JointTrajectory &trajectory) {
    bool ok = trajectory.joint_names.size() == 1 &&
              trajectory.joint_names[0] == "joint1" && trajectory.points.size() == 101;
    double peak_v = 0.0, peak_a = 0.0, peak_j = 0.0;
    double prev_t = -1.0, prev_a = 0.0;
    for (const auto &p : trajectory.points) {
      if (p.positions.size() != 1 || p.velocities.size() != 1 ||
          p.accelerations.size() != 1 || p.time_from_start.sec < 0 ||
          p.time_from_start.nanosec >= 1000000000U) {
        ok = false;
        continue;
      }
      const double t = seconds(p.time_from_start);
      const double q = p.positions[0], v = p.velocities[0], a = p.accelerations[0];
      if (!std::isfinite(t) || !std::isfinite(q) || !std::isfinite(v) || !std::isfinite(a) ||
          (prev_t >= 0.0 && t <= prev_t)) {
        ok = false;
        continue;
      }
      peak_v = std::max(peak_v, std::abs(v));
      peak_a = std::max(peak_a, std::abs(a));
      if (prev_t >= 0.0) {
        // 가속도 차분 / 시간 차분은 구간 평균 jerk: 연속 최대보다 작을 수 있다.
        peak_j = std::max(peak_j, std::abs((a - prev_a) / (t - prev_t)));
      } else if (std::abs(t) > 1e-9) {
        ok = false;
      }
      prev_t = t;
      prev_a = a;
    }
    if (ok) {
      const auto &first = trajectory.points.front();
      const auto &last = trajectory.points.back();
      const double delta = last.positions[0] - first.positions[0];
      const double abs_delta = std::abs(delta);
      // 샘플만의 최대치가 아닌 5차 다항식의 *연속시간* 극댓값을 재계산한다.
      // 먼저 모든 점이 그 식에 맞는지 확인해야 이 보수적 증명이 유효하다.
      const double continuous_v = 1.875 * abs_delta / prev_t;
      const double continuous_a = (10.0 / std::sqrt(3.0)) * abs_delta / (prev_t * prev_t);
      const double continuous_j = 60.0 * abs_delta / (prev_t * prev_t * prev_t);
      for (std::size_t i = 0; i < trajectory.points.size(); ++i) {
        const auto &p = trajectory.points[i];
        const double s = static_cast<double>(i) / 100.0;
        const double s2 = s * s, s3 = s2 * s;
        const double expected_q = first.positions[0] + delta *
            (10.0 * s3 - 15.0 * s3 * s + 6.0 * s3 * s2);
        const double expected_v = delta * 30.0 * s2 * (1.0 - s) * (1.0 - s) / prev_t;
        const double expected_a = delta * 60.0 * s * (1.0 - s) * (1.0 - 2.0 * s)
                                  / (prev_t * prev_t);
        ok = ok && std::abs(seconds(p.time_from_start) - s * prev_t) < 2e-9 &&
             std::abs(p.positions[0] - expected_q) < 1e-6 &&
             std::abs(p.velocities[0] - expected_v) < 1e-6 &&
             std::abs(p.accelerations[0] - expected_a) < 1e-6;
      }
      // 정지→정지 경계조건, 관측 표본, 연속 다항식의 세 제한을 모두 확인한다.
      ok = prev_t >= 0.5 && std::abs(first.velocities[0]) < 1e-8 &&
           std::abs(last.velocities[0]) < 1e-8 &&
           std::abs(first.accelerations[0]) < 1e-8 &&
           std::abs(last.accelerations[0]) < 1e-8 &&
           peak_v <= kVmax + 1e-8 && peak_a <= kAmax + 1e-8 &&
           peak_j <= kJmax + 1e-5 && continuous_v <= kVmax + 1e-8 &&
           continuous_a <= kAmax + 1e-8 && continuous_j <= kJmax + 1e-8 && ok;
    }
    std_msgs::msg::String result;
    result.data = std::string(ok ? "PASS" : "FAIL") + " points=" +
        std::to_string(trajectory.points.size()) + " peak_v=" + std::to_string(peak_v) +
        " peak_a=" + std::to_string(peak_a) + " sampled_peak_j=" + std::to_string(peak_j);
    publisher_->publish(result);
    RCLCPP_INFO(get_logger(), "%s", result.data.c_str());
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
  rclcpp::Subscription<trajectory_msgs::msg::JointTrajectory>::SharedPtr subscription_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TrajectoryAuditor>());
  rclcpp::shutdown();
  return 0;
}
