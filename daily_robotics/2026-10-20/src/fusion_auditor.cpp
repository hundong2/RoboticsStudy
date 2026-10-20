#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "daily_robotics_2026_10_20/msg/fusion_stats.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
constexpr std::size_t kTruthCapacity = 96U;

std::int64_t stamp_to_nanoseconds(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1000000000LL +
         static_cast<std::int64_t>(stamp.nanosec);
}

double quaternion_angle_error(
  const geometry_msgs::msg::Quaternion & lhs, const geometry_msgs::msg::Quaternion & rhs)
{
  const double lhs_norm = std::sqrt(
    lhs.x * lhs.x + lhs.y * lhs.y + lhs.z * lhs.z + lhs.w * lhs.w);
  const double rhs_norm = std::sqrt(
    rhs.x * rhs.x + rhs.y * rhs.y + rhs.z * rhs.z + rhs.w * rhs.w);
  if (lhs_norm < 1.0e-12 || rhs_norm < 1.0e-12) {
    return 3.14159265358979323846;
  }
  // 두 단위 쿼터니언의 내적 절댓값은 q와 -q가 같은 회전이라는 사실을 반영한다.
  const double dot = std::abs(
    (lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w) /
    (lhs_norm * rhs_norm));
  return 2.0 * std::acos(std::clamp(dot, 0.0, 1.0));
}
}  // namespace

class FusionAuditor : public rclcpp::Node
{
public:
  FusionAuditor()
  : Node("fusion_auditor")
  {
    const auto qos = rclcpp::QoS(rclcpp::KeepLast(20)).reliable();
    truth_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/sim/truth_pose", qos,
      std::bind(&FusionAuditor::on_truth, this, std::placeholders::_1));
    fused_sub_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/fusion/pose", qos,
      std::bind(&FusionAuditor::on_fused, this, std::placeholders::_1));
    stats_sub_ = create_subscription<daily_robotics_2026_10_20::msg::FusionStats>(
      "/fusion/stats", qos,
      std::bind(&FusionAuditor::on_stats, this, std::placeholders::_1));

    // transient_local은 auditor가 먼저 통과한 뒤 늦게 붙은 smoke-test subscriber도 결과를 받게 한다.
    pass_pub_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(1).reliable().transient_local());
  }

private:
  struct TruthSlot
  {
    bool valid{false};
    geometry_msgs::msg::PoseStamped message{};
  };

  struct PendingFused
  {
    bool valid{false};
    geometry_msgs::msg::PoseWithCovarianceStamped message{};
  };

  const geometry_msgs::msg::PoseStamped * find_truth(const builtin_interfaces::msg::Time & stamp) const
  {
    const auto target = stamp_to_nanoseconds(stamp);
    for (const auto & slot : truth_ring_) {
      if (slot.valid && stamp_to_nanoseconds(slot.message.header.stamp) == target) {
        return &slot.message;
      }
    }
    return nullptr;
  }

  void on_truth(const geometry_msgs::msg::PoseStamped::SharedPtr message)
  {
    // modulo index의 고정 ring은 오래된 truth를 자동 덮어써 메모리 증가를 막는다.
    truth_ring_[truth_write_index_].valid = true;
    truth_ring_[truth_write_index_].message = *message;
    truth_write_index_ = (truth_write_index_ + 1U) % kTruthCapacity;

    // DDS의 서로 다른 topic 사이에는 전역 순서가 없으므로 먼저 도착한 fused sample을 다시 맞춰 본다.
    for (auto & pending : pending_fused_) {
      if (pending.valid &&
        stamp_to_nanoseconds(pending.message.header.stamp) ==
        stamp_to_nanoseconds(message->header.stamp))
      {
        audit(pending.message, *message);
        pending.valid = false;
      }
    }
  }

  void on_fused(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr message)
  {
    const auto stamp = stamp_to_nanoseconds(message->header.stamp);
    if (stamp == last_audited_stamp_) {
      return;
    }
    const auto * truth = find_truth(message->header.stamp);
    if (truth != nullptr) {
      audit(*message, *truth);
      return;
    }
    for (auto & pending : pending_fused_) {
      if (!pending.valid) {
        pending.valid = true;
        pending.message = *message;
        return;
      }
    }
  }

  void audit(
    const geometry_msgs::msg::PoseWithCovarianceStamped & fused,
    const geometry_msgs::msg::PoseStamped & truth)
  {
    const double dx = fused.pose.pose.position.x - truth.pose.position.x;
    const double dy = fused.pose.pose.position.y - truth.pose.position.y;
    const double dz = fused.pose.pose.position.z - truth.pose.position.z;
    const double position_error = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double angle_error = quaternion_angle_error(
      fused.pose.pose.orientation, truth.pose.orientation);

    position_squared_error_sum_ += position_error * position_error;
    orientation_squared_error_sum_ += angle_error * angle_error;
    max_position_error_ = std::max(max_position_error_, position_error);
    max_orientation_error_ = std::max(max_orientation_error_, angle_error);
    ++matched_samples_;
    last_audited_stamp_ = stamp_to_nanoseconds(fused.header.stamp);

    // 최소 80개 독립 timestamp 표본과 지연 factor 5개를 본 뒤 구조·정확도·실행 진단을 함께 판정한다.
    if (!result_published_ && matched_samples_ >= 80U && last_stats_.delayed_factors_inserted >= 5U) {
      const double position_rmse = std::sqrt(
        position_squared_error_sum_ / static_cast<double>(matched_samples_));
      const double orientation_rmse = std::sqrt(
        orientation_squared_error_sum_ / static_cast<double>(matched_samples_));
      const bool structure_ok =
        last_stats_.active_states == 8U && last_stats_.linear_dimension == 48U &&
        last_stats_.gauss_newton_iterations == 3U;
      const bool diagnostics_ok =
        last_stats_.solve_failures == 0U && last_stats_.too_old_factors_dropped == 0U;
      const bool accuracy_ok =
        position_rmse < 0.090 && orientation_rmse < 0.080 &&
        max_position_error_ < 0.180 && max_orientation_error_ < 0.160;
      const bool passed = structure_ok && diagnostics_ok && accuracy_ok;

      std_msgs::msg::Bool result;
      result.data = passed;
      pass_pub_->publish(result);
      result_published_ = true;
      RCLCPP_INFO(
        get_logger(),
        "AUDIT %s samples=%u position_rmse=%.6f m orientation_rmse=%.6f rad "
        "max_position=%.6f m max_orientation=%.6f rad delayed=%u dropped=%u failures=%u "
        "max_solve=%.3f us",
        passed ? "PASS" : "FAIL", matched_samples_, position_rmse, orientation_rmse,
        max_position_error_, max_orientation_error_, last_stats_.delayed_factors_inserted,
        last_stats_.too_old_factors_dropped, last_stats_.solve_failures,
        last_stats_.max_solve_microseconds);
    }
  }

  void on_stats(const daily_robotics_2026_10_20::msg::FusionStats::SharedPtr message)
  {
    last_stats_ = *message;
  }

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr truth_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr fused_sub_;
  rclcpp::Subscription<daily_robotics_2026_10_20::msg::FusionStats>::SharedPtr stats_sub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_pub_;

  std::array<TruthSlot, kTruthCapacity> truth_ring_{};
  std::array<PendingFused, 8> pending_fused_{};
  daily_robotics_2026_10_20::msg::FusionStats last_stats_{};
  std::size_t truth_write_index_{0U};
  std::int64_t last_audited_stamp_{std::numeric_limits<std::int64_t>::min()};
  std::uint32_t matched_samples_{0U};
  double position_squared_error_sum_{0.0};
  double orientation_squared_error_sum_{0.0};
  double max_position_error_{0.0};
  double max_orientation_error_{0.0};
  bool result_published_{false};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FusionAuditor>());
  rclcpp::shutdown();
  return 0;
}
