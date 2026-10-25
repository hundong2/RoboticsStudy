#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#include "daily_robotics_2026_10_26/msg/stereo_correspondences.hpp"
#include "daily_robotics_2026_10_26/msg/stereo_ransac_stats.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/camera_info.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr std::size_t kMaxMatches = 48U;
constexpr std::size_t kQueueSlots = 4U;
constexpr std::size_t kRansacHypotheses = 24U;
constexpr double kDisparitySigmaPx = 0.04;
constexpr double kPi = 3.14159265358979323846;

// 한 DDS producer와 한 numerical worker만 사용하는 fixed-capacity ring이다.
// release/acquire 순서로 payload pointer가 완전히 쓰인 뒤에만 consumer에게 보인다.
template<typename T, std::size_t Capacity>
class SpscRing
{
  static_assert(Capacity >= 2U, "ring needs a sentinel slot");

public:
  bool push(T value)
  {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = (head + 1U) % Capacity;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    storage_[head] = std::move(value);
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T & value)
  {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    value = std::move(storage_[tail]);
    storage_[tail].reset();
    tail_.store((tail + 1U) % Capacity, std::memory_order_release);
    return true;
  }

private:
  std::array<T, Capacity> storage_{};
  std::atomic<std::size_t> head_{0U};
  std::atomic<std::size_t> tail_{0U};
};

struct Match
{
  double previous_x{0.0};
  double previous_z{0.0};
  double current_x{0.0};
  double current_z{0.0};
  double previous_depth_sigma{0.0};
  double current_depth_sigma{0.0};
  double normalized_u{0.0};
  double normalized_v{0.0};
};

struct Transform2
{
  // p_previous = R(yaw) * p_current + [right, forward] 관계를 표현한다.
  double yaw{0.0};
  double right{0.0};
  double forward{0.0};
};

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

void transform_point(
  const Transform2 & transform, const double current_x, const double current_z,
  double & predicted_x, double & predicted_z)
{
  const double cosine = std::cos(transform.yaw);
  const double sine = std::sin(transform.yaw);
  // 2D 강체변환 [x';z']=[c -s;s c][x;z]+t를 코드로 직접 옮겼다.
  predicted_x = cosine * current_x - sine * current_z + transform.right;
  predicted_z = sine * current_x + cosine * current_z + transform.forward;
}

double inlier_threshold(const Match & match)
{
  // sigma_z=fx*B/d^2*sigma_d에서 얻은 양 frame depth 불확실성을 합성한다.
  // threshold의 상·하한은 far point 하나가 consensus를 무한히 느슨하게 만들지 않도록 한다.
  const double combined_sigma = std::hypot(
    match.previous_depth_sigma, match.current_depth_sigma);
  return std::clamp(0.025 + 2.5 * combined_sigma, 0.035, 0.18);
}

double residual_m(const Transform2 & transform, const Match & match)
{
  double predicted_x = 0.0;
  double predicted_z = 0.0;
  transform_point(transform, match.current_x, match.current_z, predicted_x, predicted_z);
  return std::hypot(
    predicted_x - match.previous_x, predicted_z - match.previous_z);
}
}  // namespace

class BoundedStereoOdometry final : public rclcpp::Node
{
public:
  BoundedStereoOdometry()
  : Node("bounded_stereo_odometry")
  {
    // DDS 수신과 수치 worker를 서로 다른 callback group에 둔다. executor thread 격리는
    // state ownership을 명확히 하지만 OS scheduler/DDS까지 포함한 hard-RT 보장은 아니다.
    input_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    worker_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions input_options;
    input_options.callback_group = input_group_;

    // CameraInfo publisher와 동일한 Reliable/Transient Local QoS로 마지막 calibration을 받는다.
    const auto calibration_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    left_info_subscription_ = create_subscription<sensor_msgs::msg::CameraInfo>(
      "/stereo/left/camera_info", calibration_qos,
      std::bind(&BoundedStereoOdometry::on_left_info, this, std::placeholders::_1), input_options);
    right_info_subscription_ = create_subscription<sensor_msgs::msg::CameraInfo>(
      "/stereo/right/camera_info", calibration_qos,
      std::bind(&BoundedStereoOdometry::on_right_info, this, std::placeholders::_1), input_options);

    observation_subscription_ =
      create_subscription<daily_robotics_2026_10_26::msg::StereoCorrespondences>(
      "/stereo/correspondences", rclcpp::SensorDataQoS(),
      std::bind(&BoundedStereoOdometry::on_observation, this, std::placeholders::_1),
      input_options);

    odometry_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/stereo/odometry", rclcpp::QoS(10).reliable());
    stats_publisher_ = create_publisher<daily_robotics_2026_10_26::msg::StereoRansacStats>(
      "/stereo/ransac_stats", rclcpp::QoS(10).reliable());

    // 100 Hz worker는 wakeup당 관측 하나만 처리한다. 과부하 시 backlog 전체를 비우며
    // callback 시간이 폭증하지 않고, fixed ring drop counter로 손실이 드러난다.
    worker_timer_ = create_wall_timer(
      10ms, std::bind(&BoundedStereoOdometry::process_one_observation, this), worker_group_);

    RCLCPP_INFO(
      get_logger(),
      "bounded stereo odometry: <=%zu matches, exactly %zu hypotheses, one packet/wakeup",
      kMaxMatches, kRansacHypotheses);
  }

private:
  using ObservationConstPtr =
    daily_robotics_2026_10_26::msg::StereoCorrespondences::ConstSharedPtr;

  // 좌 camera K 행렬의 초점거리와 주점을 검증해 atomics에 저장한다.
  void on_left_info(const sensor_msgs::msg::CameraInfo::ConstSharedPtr info)
  {
    const bool valid = info->header.frame_id == "left_camera_optical_frame" &&
      info->width > 0U && info->height > 0U && info->k[0] > 1.0 && info->k[4] > 1.0;
    if (valid) {
      fx_px_.store(info->k[0], std::memory_order_relaxed);
      fy_px_.store(info->k[4], std::memory_order_relaxed);
      cx_px_.store(info->k[2], std::memory_order_relaxed);
      cy_px_.store(info->k[5], std::memory_order_relaxed);
    }
    left_info_valid_.store(valid, std::memory_order_release);
  }

  // rectified right P의 Tx로 baseline B=-P[3]/P[0]을 복원한다.
  void on_right_info(const sensor_msgs::msg::CameraInfo::ConstSharedPtr info)
  {
    const double projection_fx = info->p[0];
    const double baseline = projection_fx > 1.0 ? -info->p[3] / projection_fx : 0.0;
    const bool valid = info->header.frame_id == "right_camera_optical_frame" &&
      projection_fx > 1.0 && baseline > 0.01 && baseline < 1.0;
    if (valid) {
      right_fx_px_.store(projection_fx, std::memory_order_relaxed);
      baseline_m_.store(baseline, std::memory_order_relaxed);
    }
    right_info_valid_.store(valid, std::memory_order_release);
  }

  // DDS callback에서는 수치 계산/복사를 하지 않고 shared pointer만 SPSC ring에 넘긴다.
  void on_observation(ObservationConstPtr observation)
  {
    if (!observation_queue_.push(std::move(observation))) {
      observation_queue_drops_.fetch_add(1U, std::memory_order_relaxed);
    }
  }

  // disparity d로 Z=fx*B/d, X=(u-cx)Z/fx를 복원하고 Jacobian으로 depth sigma를 계산한다.
  std::size_t reconstruct_matches(
    const daily_robotics_2026_10_26::msg::StereoCorrespondences & observation,
    const double fx, const double fy, const double cx, const double cy, const double baseline,
    double & maximum_depth_sigma)
  {
    const auto count = std::min<std::size_t>(observation.count, kMaxMatches);
    std::size_t valid_count = 0U;
    maximum_depth_sigma = 0.0;
    for (std::size_t i = 0U; i < count; ++i) {
      const double previous_d = observation.previous_disparity[i];
      const double current_d = observation.current_disparity[i];
      if (!std::isfinite(previous_d) || !std::isfinite(current_d) ||
        previous_d <= 0.5 || current_d <= 0.5)
      {
        continue;
      }

      const double previous_z = fx * baseline / previous_d;
      const double current_z = fx * baseline / current_d;
      if (previous_z < 0.3 || previous_z > 30.0 || current_z < 0.3 || current_z > 30.0) {
        continue;
      }

      // Z=fxB/d를 d로 미분하면 |dZ/dd|=fxB/d^2다.
      // sigma_Z≈|dZ/dd|sigma_d가 disparity가 작은 먼 점의 큰 depth 불확실성을 설명한다.
      const double previous_sigma = fx * baseline * kDisparitySigmaPx /
        (previous_d * previous_d);
      const double current_sigma = fx * baseline * kDisparitySigmaPx /
        (current_d * current_d);

      matches_[valid_count] = Match{
        (observation.previous_u[i] - cx) * previous_z / fx,
        previous_z,
        (observation.current_u[i] - cx) * current_z / fx,
        current_z,
        previous_sigma,
        current_sigma,
        (observation.current_u[i] - cx) / fx,
        (observation.current_v[i] - cy) / fy};
      maximum_depth_sigma = std::max(
        maximum_depth_sigma, std::max(previous_sigma, current_sigma));
      ++valid_count;
    }
    return valid_count;
  }

  // 두 correspondence만으로 current→previous SE(2) 강체변환 hypothesis를 만든다.
  bool fit_pair(const Match & first, const Match & second, Transform2 & transform) const
  {
    const double current_dx = second.current_x - first.current_x;
    const double current_dz = second.current_z - first.current_z;
    const double previous_dx = second.previous_x - first.previous_x;
    const double previous_dz = second.previous_z - first.previous_z;
    if (std::hypot(current_dx, current_dz) < 0.15 ||
      std::hypot(previous_dx, previous_dz) < 0.15)
    {
      return false;
    }

    // 두 점을 잇는 vector angle 차이가 2D 회전각이다.
    transform.yaw = normalize_angle(
      std::atan2(previous_dz, previous_dx) - std::atan2(current_dz, current_dx));
    const double cosine = std::cos(transform.yaw);
    const double sine = std::sin(transform.yaw);
    transform.right = first.previous_x -
      (cosine * first.current_x - sine * first.current_z);
    transform.forward = first.previous_z -
      (sine * first.current_x + cosine * first.current_z);
    return true;
  }

  // fixed 24개 hypothesis 각각을 최대 48개 match에 대해 평가한다.
  Transform2 run_bounded_ransac(
    const std::size_t count, std::uint32_t & best_inlier_count,
    std::uint32_t & hypotheses_evaluated)
  {
    Transform2 best_transform;
    double best_normalized_cost = std::numeric_limits<double>::infinity();
    best_inlier_count = 0U;
    hypotheses_evaluated = 0U;

    for (std::size_t hypothesis = 0U; hypothesis < kRansacHypotheses; ++hypothesis) {
      // RNG 대신 sequence가 고정된 low-cost deterministic pair schedule을 사용한다.
      // 실행시간과 재현성은 고정되지만 adversarial ordering에는 취약하다는 trade-off가 있다.
      const std::size_t first_index = (hypothesis * 5U + 1U) % count;
      std::size_t second_index = (hypothesis * 11U + 7U) % count;
      if (second_index == first_index) {
        second_index = (second_index + 1U) % count;
      }

      Transform2 candidate;
      if (!fit_pair(matches_[first_index], matches_[second_index], candidate)) {
        continue;
      }
      ++hypotheses_evaluated;

      std::uint32_t inliers = 0U;
      double normalized_cost = 0.0;
      for (std::size_t i = 0U; i < count; ++i) {
        const double threshold = inlier_threshold(matches_[i]);
        const double normalized = residual_m(candidate, matches_[i]) / threshold;
        if (normalized <= 1.0) {
          ++inliers;
          normalized_cost += normalized * normalized;
        }
      }
      if (inliers > best_inlier_count ||
        (inliers == best_inlier_count && normalized_cost < best_normalized_cost))
      {
        best_inlier_count = inliers;
        best_normalized_cost = normalized_cost;
        best_transform = candidate;
      }
    }
    return best_transform;
  }

  // consensus inlier 전체의 centroid/cross-covariance로 SE(2)를 한 번 closed-form 재추정한다.
  Transform2 refine_consensus(
    const Transform2 & seed, const std::size_t count, std::uint32_t & inlier_count,
    double & normalized_rmse, double & spatial_eigen_ratio)
  {
    inlier_mask_.fill(false);
    double current_mean_x = 0.0;
    double current_mean_z = 0.0;
    double previous_mean_x = 0.0;
    double previous_mean_z = 0.0;
    inlier_count = 0U;
    for (std::size_t i = 0U; i < count; ++i) {
      if (residual_m(seed, matches_[i]) <= inlier_threshold(matches_[i])) {
        inlier_mask_[i] = true;
        current_mean_x += matches_[i].current_x;
        current_mean_z += matches_[i].current_z;
        previous_mean_x += matches_[i].previous_x;
        previous_mean_z += matches_[i].previous_z;
        ++inlier_count;
      }
    }

    if (inlier_count < 2U) {
      normalized_rmse = std::numeric_limits<double>::infinity();
      spatial_eigen_ratio = 0.0;
      return seed;
    }

    const double inverse_count = 1.0 / static_cast<double>(inlier_count);
    current_mean_x *= inverse_count;
    current_mean_z *= inverse_count;
    previous_mean_x *= inverse_count;
    previous_mean_z *= inverse_count;

    double dot = 0.0;
    double cross = 0.0;
    for (std::size_t i = 0U; i < count; ++i) {
      if (!inlier_mask_[i]) {
        continue;
      }
      const double current_x = matches_[i].current_x - current_mean_x;
      const double current_z = matches_[i].current_z - current_mean_z;
      const double previous_x = matches_[i].previous_x - previous_mean_x;
      const double previous_z = matches_[i].previous_z - previous_mean_z;
      dot += current_x * previous_x + current_z * previous_z;
      cross += current_x * previous_z - current_z * previous_x;
    }

    Transform2 refined;
    refined.yaw = std::atan2(cross, dot);
    const double cosine = std::cos(refined.yaw);
    const double sine = std::sin(refined.yaw);
    refined.right = previous_mean_x -
      (cosine * current_mean_x - sine * current_mean_z);
    refined.forward = previous_mean_z -
      (sine * current_mean_x + cosine * current_mean_z);

    // 최종 model의 normalized residual과 image-plane 2x2 covariance eigenvalue ratio를 계산한다.
    double normalized_squared_sum = 0.0;
    double mean_u = 0.0;
    double mean_v = 0.0;
    for (std::size_t i = 0U; i < count; ++i) {
      if (!inlier_mask_[i]) {
        continue;
      }
      const double normalized = residual_m(refined, matches_[i]) / inlier_threshold(matches_[i]);
      normalized_squared_sum += normalized * normalized;
      mean_u += matches_[i].normalized_u;
      mean_v += matches_[i].normalized_v;
    }
    mean_u *= inverse_count;
    mean_v *= inverse_count;

    double covariance_uu = 0.0;
    double covariance_uv = 0.0;
    double covariance_vv = 0.0;
    for (std::size_t i = 0U; i < count; ++i) {
      if (!inlier_mask_[i]) {
        continue;
      }
      const double du = matches_[i].normalized_u - mean_u;
      const double dv = matches_[i].normalized_v - mean_v;
      covariance_uu += du * du;
      covariance_uv += du * dv;
      covariance_vv += dv * dv;
    }
    covariance_uu *= inverse_count;
    covariance_uv *= inverse_count;
    covariance_vv *= inverse_count;

    // 2x2 대칭행렬의 closed-form eigenvalue다. lambda_min/lambda_max가 0에 가까우면
    // feature가 한 선에 몰려 회전/이동 추정이 작은 잡음에도 민감한 공간 퇴화 상태다.
    const double trace = covariance_uu + covariance_vv;
    const double discriminant = std::sqrt(
      (covariance_uu - covariance_vv) * (covariance_uu - covariance_vv) +
      4.0 * covariance_uv * covariance_uv);
    const double lambda_max = 0.5 * (trace + discriminant);
    const double lambda_min = std::max(0.0, 0.5 * (trace - discriminant));
    spatial_eigen_ratio = lambda_max > 1.0e-12 ? lambda_min / lambda_max : 0.0;
    normalized_rmse = std::sqrt(normalized_squared_sum * inverse_count);
    return refined;
  }

  // 한 packet을 reconstruction→fixed RANSAC→refinement→pose composition 순으로 처리한다.
  void process_one_observation()
  {
    ObservationConstPtr observation;
    if (!observation_queue_.pop(observation)) {
      return;
    }

    const auto start = std::chrono::steady_clock::now();
    const bool left_valid = left_info_valid_.load(std::memory_order_acquire);
    const bool right_valid = right_info_valid_.load(std::memory_order_acquire);
    const double fx = fx_px_.load(std::memory_order_relaxed);
    const double fy = fy_px_.load(std::memory_order_relaxed);
    const double cx = cx_px_.load(std::memory_order_relaxed);
    const double cy = cy_px_.load(std::memory_order_relaxed);
    const double right_fx = right_fx_px_.load(std::memory_order_relaxed);
    const double baseline = baseline_m_.load(std::memory_order_relaxed);
    const bool calibration_valid = left_valid && right_valid &&
      std::abs(fx - right_fx) < 1.0e-6 && baseline > 0.0;
    const bool message_valid = observation->header.frame_id == "left_camera_optical_frame" &&
      observation->count <= kMaxMatches;
    if (!calibration_valid || !message_valid) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000,
        "stereo calibration or correspondence contract invalid; packet skipped");
      return;
    }

    double maximum_depth_sigma = 0.0;
    const std::size_t valid_count = reconstruct_matches(
      *observation, fx, fy, cx, cy, baseline, maximum_depth_sigma);
    if (valid_count < 8U) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000, "fewer than 8 valid stereo depths; packet skipped");
      return;
    }

    std::uint32_t seed_inliers = 0U;
    std::uint32_t hypotheses_evaluated = 0U;
    const Transform2 seed = run_bounded_ransac(
      valid_count, seed_inliers, hypotheses_evaluated);
    std::uint32_t inlier_count = 0U;
    double normalized_rmse = 0.0;
    double spatial_eigen_ratio = 0.0;
    const Transform2 relative = refine_consensus(
      seed, valid_count, inlier_count, normalized_rmse, spatial_eigen_ratio);

    const bool spatial_geometry_valid = spatial_eigen_ratio >= 0.015;
    const bool estimate_valid = inlier_count >= 20U && spatial_geometry_valid &&
      std::isfinite(normalized_rmse) && normalized_rmse <= 1.0;
    if (estimate_valid) {
      // T_world_current = T_world_previous * T_previous_current를 누적한다.
      const double cosine = std::cos(world_yaw_);
      const double sine = std::sin(world_yaw_);
      world_right_ += cosine * relative.right - sine * relative.forward;
      world_forward_ += sine * relative.right + cosine * relative.forward;
      world_yaw_ = normalize_angle(world_yaw_ + relative.yaw);
    }

    const auto end = std::chrono::steady_clock::now();
    const double kernel_us = std::chrono::duration<double, std::micro>(end - start).count();
    max_kernel_us_ = std::max(max_kernel_us_, kernel_us);

    nav_msgs::msg::Odometry odometry;
    odometry.header = observation->header;
    odometry.header.frame_id = "odom";
    odometry.child_frame_id = "base_link";
    // optical right/forward를 base left/forward로 바꿔 ROS 평면 pose (+x forward,+y left)에 기록한다.
    odometry.pose.pose.position.x = world_forward_;
    odometry.pose.pose.position.y = -world_right_;
    odometry.pose.pose.orientation.z = std::sin(0.5 * world_yaw_);
    odometry.pose.pose.orientation.w = std::cos(0.5 * world_yaw_);
    odometry.twist.twist.linear.x = relative.forward * 30.0;
    odometry.twist.twist.linear.y = -relative.right * 30.0;
    odometry.twist.twist.angular.z = relative.yaw * 30.0;
    // 6x6 row-major covariance에서 x,y,yaw 대각만 교육용 residual 기반 값으로 채운다.
    const double variance = std::max(1.0e-6, normalized_rmse * normalized_rmse * 1.0e-4);
    odometry.pose.covariance[0] = variance;
    odometry.pose.covariance[7] = variance;
    odometry.pose.covariance[35] = variance;

    daily_robotics_2026_10_26::msg::StereoRansacStats stats;
    stats.header = observation->header;
    stats.sequence = observation->sequence;
    stats.input_count = observation->count;
    stats.valid_depth_count = static_cast<std::uint32_t>(valid_count);
    stats.inlier_count = inlier_count;
    stats.rejected_count = static_cast<std::uint32_t>(valid_count) - inlier_count;
    stats.hypotheses_evaluated = hypotheses_evaluated;
    stats.observation_queue_drops = observation_queue_drops_.load(std::memory_order_relaxed);
    stats.estimated_forward_delta = relative.forward;
    stats.estimated_lateral_delta = -relative.right;
    stats.estimated_yaw_delta = relative.yaw;
    stats.accumulated_forward = world_forward_;
    stats.accumulated_lateral = -world_right_;
    stats.accumulated_yaw = world_yaw_;
    stats.maximum_depth_sigma = maximum_depth_sigma;
    stats.normalized_residual_rmse = normalized_rmse;
    stats.spatial_eigen_ratio = spatial_eigen_ratio;
    stats.latest_kernel_us = kernel_us;
    stats.max_kernel_us = max_kernel_us_;
    stats.calibration_valid = calibration_valid;
    stats.spatial_geometry_valid = spatial_geometry_valid;
    stats.bounds_respected = observation->count <= kMaxMatches &&
      hypotheses_evaluated <= kRansacHypotheses;

    odometry_publisher_->publish(odometry);
    stats_publisher_->publish(stats);
  }

  rclcpp::CallbackGroup::SharedPtr input_group_;
  rclcpp::CallbackGroup::SharedPtr worker_group_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr left_info_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr right_info_subscription_;
  rclcpp::Subscription<daily_robotics_2026_10_26::msg::StereoCorrespondences>::SharedPtr
    observation_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odometry_publisher_;
  rclcpp::Publisher<daily_robotics_2026_10_26::msg::StereoRansacStats>::SharedPtr
    stats_publisher_;
  rclcpp::TimerBase::SharedPtr worker_timer_;

  SpscRing<ObservationConstPtr, kQueueSlots> observation_queue_;
  std::array<Match, kMaxMatches> matches_{};
  std::array<bool, kMaxMatches> inlier_mask_{};
  std::atomic<std::uint32_t> observation_queue_drops_{0U};

  std::atomic<double> fx_px_{0.0};
  std::atomic<double> fy_px_{0.0};
  std::atomic<double> cx_px_{0.0};
  std::atomic<double> cy_px_{0.0};
  std::atomic<double> right_fx_px_{0.0};
  std::atomic<double> baseline_m_{0.0};
  std::atomic<bool> left_info_valid_{false};
  std::atomic<bool> right_info_valid_{false};

  double world_right_{0.0};
  double world_forward_{0.0};
  double world_yaw_{0.0};
  double max_kernel_us_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<BoundedStereoOdometry>();
  // 두 thread가 input/worker callback group을 병렬 처리한다. callback group의 MutuallyExclusive
  // 규칙 때문에 각 group 내부 state는 동시에 두 callback이 수정하지 않는다.
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2U);
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
