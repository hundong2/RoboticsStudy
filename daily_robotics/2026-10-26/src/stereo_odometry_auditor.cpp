#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "daily_robotics_2026_10_26/msg/stereo_ransac_stats.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
constexpr std::size_t kAuditSlots = 256U;
constexpr double kPi = 3.14159265358979323846;

// topic 간 도착 순서를 가정하지 않고 acquisition stamp를 key로 결합하는 fixed-capacity 저장소다.
template<typename MessageT, std::size_t Capacity>
class StampRing
{
public:
  void insert(const std::int64_t stamp_ns, const MessageT & message)
  {
    for (auto & slot : slots_) {
      if (slot.valid && slot.stamp_ns == stamp_ns) {
        slot.message = message;
        return;
      }
    }
    slots_[next_] = Slot{stamp_ns, message, true};
    next_ = (next_ + 1U) % Capacity;
  }

  MessageT * find(const std::int64_t stamp_ns)
  {
    for (auto & slot : slots_) {
      if (slot.valid && slot.stamp_ns == stamp_ns) {
        return &slot.message;
      }
    }
    return nullptr;
  }

  void erase(const std::int64_t stamp_ns)
  {
    for (auto & slot : slots_) {
      if (slot.valid && slot.stamp_ns == stamp_ns) {
        slot.valid = false;
        return;
      }
    }
  }

private:
  struct Slot
  {
    std::int64_t stamp_ns{0};
    MessageT message{};
    bool valid{false};
  };

  std::array<Slot, Capacity> slots_{};
  std::size_t next_{0U};
};

std::int64_t stamp_to_nanoseconds(const builtin_interfaces::msg::Time & stamp)
{
  // rclcpp::Time이 sec/nanosec를 하나의 signed nanosecond key로 변환한다.
  return rclcpp::Time(stamp).nanoseconds();
}

double yaw_from_quaternion(const geometry_msgs::msg::Quaternion & quaternion)
{
  // planar quaternion q=[0,0,sin(yaw/2),cos(yaw/2)]의 일반 yaw 추출식이다.
  const double sine = 2.0 *
    (quaternion.w * quaternion.z + quaternion.x * quaternion.y);
  const double cosine = 1.0 - 2.0 *
    (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
  return std::atan2(sine, cosine);
}

double normalize_angle(double angle)
{
  while (angle > kPi) {
    angle -= 2.0 * kPi;
  }
  while (angle < -kPi) {
    angle += 2.0 * kPi;
  }
  return angle;
}
}  // namespace

class StereoOdometryAuditor final : public rclcpp::Node
{
public:
  StereoOdometryAuditor()
  : Node("stereo_odometry_auditor")
  {
    // 세 subscriber는 도착 순서를 믿지 않는다. 동일 stamp의 truth/odometry/stats가 모두
    // 도착했을 때만 한 sample로 인정한다.
    truth_subscription_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/sim/stereo_truth", rclcpp::QoS(10).reliable(),
      [this](const geometry_msgs::msg::PoseStamped::ConstSharedPtr message) {
        const auto stamp = stamp_to_nanoseconds(message->header.stamp);
        truths_.insert(stamp, *message);
        try_audit(stamp);
      });
    odometry_subscription_ = create_subscription<nav_msgs::msg::Odometry>(
      "/stereo/odometry", rclcpp::QoS(10).reliable(),
      [this](const nav_msgs::msg::Odometry::ConstSharedPtr message) {
        const auto stamp = stamp_to_nanoseconds(message->header.stamp);
        odometries_.insert(stamp, *message);
        try_audit(stamp);
      });
    stats_subscription_ =
      create_subscription<daily_robotics_2026_10_26::msg::StereoRansacStats>(
      "/stereo/ransac_stats", rclcpp::QoS(10).reliable(),
      [this](const daily_robotics_2026_10_26::msg::StereoRansacStats::ConstSharedPtr message) {
        const auto stamp = stamp_to_nanoseconds(message->header.stamp);
        statistics_.insert(stamp, *message);
        try_audit(stamp);
      });

    // Transient Local 결과는 smoke_test가 늦게 subscribe해도 마지막 pass/fail을 받게 한다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
  }

private:
  // 동일 stamp 세 메시지를 독립 acceptance 기준으로 검증한다.
  void try_audit(const std::int64_t stamp_ns)
  {
    auto * truth = truths_.find(stamp_ns);
    auto * odometry = odometries_.find(stamp_ns);
    auto * stats = statistics_.find(stamp_ns);
    if (truth == nullptr || odometry == nullptr || stats == nullptr) {
      return;
    }

    const double truth_yaw = yaw_from_quaternion(truth->pose.orientation);
    const double estimate_yaw = yaw_from_quaternion(odometry->pose.pose.orientation);

    // VO의 global origin은 gauge freedom이므로 첫 exact-stamp pair에서 x/y/yaw origin만 맞춘다.
    // 이후 drift는 보정하지 않아 누적 오차가 그대로 acceptance metric에 들어간다.
    if (!origin_aligned_) {
      origin_x_offset_ = truth->pose.position.x - odometry->pose.pose.position.x;
      origin_y_offset_ = truth->pose.position.y - odometry->pose.pose.position.y;
      origin_yaw_offset_ = normalize_angle(truth_yaw - estimate_yaw);
      origin_aligned_ = true;
    }

    const double error_x = odometry->pose.pose.position.x + origin_x_offset_ -
      truth->pose.position.x;
    const double error_y = odometry->pose.pose.position.y + origin_y_offset_ -
      truth->pose.position.y;
    const double position_error = std::hypot(error_x, error_y);
    const double yaw_error = normalize_angle(
      estimate_yaw + origin_yaw_offset_ - truth_yaw);
    position_squared_error_sum_ += position_error * position_error;
    yaw_squared_error_sum_ += yaw_error * yaw_error;
    max_position_error_ = std::max(max_position_error_, position_error);
    max_yaw_error_ = std::max(max_yaw_error_, std::abs(yaw_error));
    minimum_inliers_ = std::min(minimum_inliers_, stats->inlier_count);
    minimum_spatial_ratio_ = std::min(minimum_spatial_ratio_, stats->spatial_eigen_ratio);
    maximum_normalized_rmse_ = std::max(
      maximum_normalized_rmse_, stats->normalized_residual_rmse);
    maximum_depth_sigma_ = std::max(maximum_depth_sigma_, stats->maximum_depth_sigma);
    max_kernel_us_ = std::max(max_kernel_us_, stats->max_kernel_us);
    max_queue_drops_ = std::max(max_queue_drops_, stats->observation_queue_drops);
    minimum_hypotheses_ = std::min(minimum_hypotheses_, stats->hypotheses_evaluated);
    all_calibrations_valid_ = all_calibrations_valid_ && stats->calibration_valid;
    all_spatial_geometry_valid_ = all_spatial_geometry_valid_ && stats->spatial_geometry_valid;
    all_bounds_respected_ = all_bounds_respected_ && stats->bounds_respected;
    ++audited_samples_;

    truths_.erase(stamp_ns);
    odometries_.erase(stamp_ns);
    statistics_.erase(stamp_ns);

    if (!result_published_ && audited_samples_ >= 100U) {
      const double inverse_count = 1.0 / static_cast<double>(audited_samples_);
      const double position_rmse = std::sqrt(position_squared_error_sum_ * inverse_count);
      const double yaw_rmse = std::sqrt(yaw_squared_error_sum_ * inverse_count);
      const bool pass = position_rmse < 0.070 && max_position_error_ < 0.150 &&
        yaw_rmse < 0.020 && max_yaw_error_ < 0.040 && minimum_inliers_ >= 36U &&
        minimum_spatial_ratio_ >= 0.015 && maximum_normalized_rmse_ <= 1.0 &&
        minimum_hypotheses_ >= 20U && max_queue_drops_ == 0U &&
        all_calibrations_valid_ && all_spatial_geometry_valid_ && all_bounds_respected_;

      std_msgs::msg::Bool result;
      result.data = pass;
      pass_publisher_->publish(result);
      result_published_ = true;

      if (pass) {
        RCLCPP_INFO(
          get_logger(),
          "AUDIT PASS samples=%u position_rmse=%.6f m max_position=%.6f m "
          "yaw_rmse=%.6f rad max_yaw=%.6f rad min_inliers=%u min_spatial=%.4f "
          "max_norm_rmse=%.4f max_depth_sigma=%.4f m max_kernel=%.3f us drops=%u",
          audited_samples_, position_rmse, max_position_error_, yaw_rmse, max_yaw_error_,
          minimum_inliers_, minimum_spatial_ratio_, maximum_normalized_rmse_,
          maximum_depth_sigma_, max_kernel_us_, max_queue_drops_);
      } else {
        RCLCPP_ERROR(
          get_logger(),
          "AUDIT FAIL samples=%u position_rmse=%.6f max_position=%.6f yaw_rmse=%.6f "
          "max_yaw=%.6f min_inliers=%u min_spatial=%.4f max_norm_rmse=%.4f "
          "min_hypotheses=%u calibration=%s geometry=%s bounds=%s drops=%u",
          audited_samples_, position_rmse, max_position_error_, yaw_rmse, max_yaw_error_,
          minimum_inliers_, minimum_spatial_ratio_, maximum_normalized_rmse_,
          minimum_hypotheses_, all_calibrations_valid_ ? "true" : "false",
          all_spatial_geometry_valid_ ? "true" : "false",
          all_bounds_respected_ ? "true" : "false", max_queue_drops_);
      }
    }
  }

  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr truth_subscription_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometry_subscription_;
  rclcpp::Subscription<daily_robotics_2026_10_26::msg::StereoRansacStats>::SharedPtr
    stats_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;

  StampRing<geometry_msgs::msg::PoseStamped, kAuditSlots> truths_;
  StampRing<nav_msgs::msg::Odometry, kAuditSlots> odometries_;
  StampRing<daily_robotics_2026_10_26::msg::StereoRansacStats, kAuditSlots> statistics_;

  bool origin_aligned_{false};
  bool result_published_{false};
  bool all_calibrations_valid_{true};
  bool all_spatial_geometry_valid_{true};
  bool all_bounds_respected_{true};
  double origin_x_offset_{0.0};
  double origin_y_offset_{0.0};
  double origin_yaw_offset_{0.0};
  double position_squared_error_sum_{0.0};
  double yaw_squared_error_sum_{0.0};
  double max_position_error_{0.0};
  double max_yaw_error_{0.0};
  double minimum_spatial_ratio_{std::numeric_limits<double>::infinity()};
  double maximum_normalized_rmse_{0.0};
  double maximum_depth_sigma_{0.0};
  double max_kernel_us_{0.0};
  std::uint32_t minimum_inliers_{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t minimum_hypotheses_{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t max_queue_drops_{0U};
  std::uint32_t audited_samples_{0U};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // auditor callback는 작은 fixed-ring join만 수행하므로 single-threaded spin으로 ownership을 단순화한다.
  rclcpp::spin(std::make_shared<StereoOdometryAuditor>());
  rclcpp::shutdown();
  return 0;
}
