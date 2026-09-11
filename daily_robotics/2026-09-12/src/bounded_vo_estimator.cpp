#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>

#include "geometry_msgs/msg/quaternion.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/channel_float32.hpp"
#include "sensor_msgs/msg/point_cloud.hpp"
#include "std_msgs/msg/string.hpp"

namespace daily_robotics
{

constexpr std::size_t kMaxFeatures = 32U;
constexpr double kOutlierThresholdM = 0.15;

struct Point2
{
  double x{};
  double y{};
};

// Frame은 VO 수학 hot path가 사용하는 완전히 고정 크기 작업 공간이다.
struct Frame
{
  std::array<Point2, kMaxFeatures> points{};
  std::array<std::uint32_t, kMaxFeatures> ids{};
  std::size_t count{0U};
  builtin_interfaces::msg::Time stamp{};
};

struct Transform2
{
  double x{};
  double y{};
  double yaw{};
};

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

geometry_msgs::msg::Quaternion yaw_to_quaternion(const double yaw)
{
  geometry_msgs::msg::Quaternion q;
  q.z = std::sin(0.5 * yaw);
  q.w = std::cos(0.5 * yaw);
  return q;
}

// 현재 frame 점을 이전 frame 점에 맞추는 2D Kabsch/Procrustes closed form이다.
// 목적함수는 Σ||p_prev - (R(θ)p_cur+t)||²이며, atan2(cross,dot)로 최적 θ를 얻는다.
bool fit_se2(
  const std::array<Point2, kMaxFeatures> & current,
  const std::array<Point2, kMaxFeatures> & previous,
  const std::array<bool, kMaxFeatures> & use,
  const std::size_t pair_count,
  Transform2 & transform,
  std::size_t & used_count)
{
  Point2 current_center;
  Point2 previous_center;
  used_count = 0U;
  for (std::size_t i = 0; i < pair_count; ++i) {
    if (!use[i]) {
      continue;
    }
    current_center.x += current[i].x;
    current_center.y += current[i].y;
    previous_center.x += previous[i].x;
    previous_center.y += previous[i].y;
    ++used_count;
  }
  if (used_count < 3U) {
    return false;
  }

  const double inv_count = 1.0 / static_cast<double>(used_count);
  current_center.x *= inv_count;
  current_center.y *= inv_count;
  previous_center.x *= inv_count;
  previous_center.y *= inv_count;

  double dot_sum = 0.0;
  double cross_sum = 0.0;
  for (std::size_t i = 0; i < pair_count; ++i) {
    if (!use[i]) {
      continue;
    }
    const double cx = current[i].x - current_center.x;
    const double cy = current[i].y - current_center.y;
    const double px = previous[i].x - previous_center.x;
    const double py = previous[i].y - previous_center.y;
    dot_sum += cx * px + cy * py;
    cross_sum += cx * py - cy * px;
  }

  transform.yaw = std::atan2(cross_sum, dot_sum);
  const double c = std::cos(transform.yaw);
  const double s = std::sin(transform.yaw);
  // centroid 관계 c_prev=R*c_cur+t를 정리한 t=c_prev-R*c_cur다.
  transform.x = previous_center.x - (c * current_center.x - s * current_center.y);
  transform.y = previous_center.y - (s * current_center.x + c * current_center.y);
  return true;
}

double residual(const Transform2 & transform, const Point2 & current, const Point2 & previous)
{
  const double c = std::cos(transform.yaw);
  const double s = std::sin(transform.yaw);
  const double predicted_x = c * current.x - s * current.y + transform.x;
  const double predicted_y = s * current.x + c * current.y + transform.y;
  return std::hypot(previous.x - predicted_x, previous.y - predicted_y);
}

// 이 노드는 연속 metric feature frame의 공통 ID를 찾아 SE(2) motion을 계산하고 odometry로 누적한다.
class BoundedVoEstimator final : public rclcpp::Node
{
public:
  BoundedVoEstimator()
  : Node("bounded_vo_estimator")
  {
    odometry_pub_ = create_publisher<nav_msgs::msg::Odometry>(
      "/vo/odometry", rclcpp::QoS(10).reliable());
    diagnostics_pub_ = create_publisher<std_msgs::msg::String>(
      "/vo/diagnostics", rclcpp::QoS(10).reliable());

    // SensorDataQoS를 써서 upstream best-effort publisher와 호환한다.
    feature_sub_ = create_subscription<sensor_msgs::msg::PointCloud>(
      "/sync/metric_features", rclcpp::SensorDataQoS(),
      [this](sensor_msgs::msg::PointCloud::ConstSharedPtr message) { on_features(*message); });
  }

private:
  static const sensor_msgs::msg::ChannelFloat32 * find_id_channel(
    const sensor_msgs::msg::PointCloud & cloud)
  {
    for (const auto & channel : cloud.channels) {
      if (channel.name == "feature_id") {
        return &channel;
      }
    }
    return nullptr;
  }

  // ROS의 가변 길이 PointCloud를 최대 32개 고정 Frame으로 옮긴다.
  // 이 경계 뒤의 matching, fitting, trimming은 heap allocation 없이 정해진 배열만 사용한다.
  static bool copy_bounded(const sensor_msgs::msg::PointCloud & message, Frame & frame)
  {
    const auto * ids = find_id_channel(message);
    if (ids == nullptr || message.points.size() < 3U || message.points.size() > kMaxFeatures ||
      ids->values.size() < message.points.size())
    {
      return false;
    }

    frame.count = message.points.size();
    frame.stamp = message.header.stamp;
    for (std::size_t i = 0; i < frame.count; ++i) {
      frame.points[i] = Point2{message.points[i].x, message.points[i].y};
      frame.ids[i] = static_cast<std::uint32_t>(std::lround(ids->values[i]));
    }
    return true;
  }

  // Feature callback은 동기화된 frame 하나로 camera motion을 추정한다.
  void on_features(const sensor_msgs::msg::PointCloud & message)
  {
    const auto started_at = std::chrono::steady_clock::now();
    Frame current;
    if (!copy_bounded(message, current)) {
      RCLCPP_WARN(get_logger(), "PointCloud bounded contract 위반; frame을 버립니다");
      return;
    }

    if (!have_previous_) {
      previous_ = current;
      have_previous_ = true;
      publish_odometry(current.stamp);
      publish_diagnostics("initialized", current.count, current.count, 0.0, 0L);
      return;
    }

    std::array<Point2, kMaxFeatures> matched_current{};
    std::array<Point2, kMaxFeatures> matched_previous{};
    std::array<bool, kMaxFeatures> use{};
    std::size_t pair_count = 0U;

    // 최대 32x32 비교로 같은 feature_id를 찾는다. O(N²)이지만 N 상한이 고정되어 실행 상한도 고정된다.
    for (std::size_t current_i = 0; current_i < current.count; ++current_i) {
      for (std::size_t previous_i = 0; previous_i < previous_.count; ++previous_i) {
        if (current.ids[current_i] == previous_.ids[previous_i]) {
          matched_current[pair_count] = current.points[current_i];
          matched_previous[pair_count] = previous_.points[previous_i];
          use[pair_count] = true;
          ++pair_count;
          break;
        }
      }
    }

    Transform2 motion;
    std::size_t used_count = 0U;
    if (!fit_se2(matched_current, matched_previous, use, pair_count, motion, used_count)) {
      RCLCPP_WARN(get_logger(), "공통 feature가 부족해 motion update를 건너뜁니다");
      previous_ = current;
      return;
    }

    // 1차 fit residual이 15 cm를 넘는 대응점을 제거한다. 고정 2-pass라 반복 횟수가 입력에 따라 늘지 않는다.
    for (std::size_t i = 0; i < pair_count; ++i) {
      use[i] = residual(motion, matched_current[i], matched_previous[i]) <= kOutlierThresholdM;
    }
    if (!fit_se2(matched_current, matched_previous, use, pair_count, motion, used_count)) {
      RCLCPP_WARN(get_logger(), "outlier 제거 뒤 inlier가 부족해 motion update를 건너뜁니다");
      previous_ = current;
      return;
    }

    double squared_error_sum = 0.0;
    for (std::size_t i = 0; i < pair_count; ++i) {
      if (use[i]) {
        const double error = residual(motion, matched_current[i], matched_previous[i]);
        squared_error_sum += error * error;
      }
    }
    const double rmse = std::sqrt(squared_error_sum / static_cast<double>(used_count));

    // motion.t는 이전 robot frame에 표현된 Δposition이므로 현재 global yaw로 world frame에 회전해 누적한다.
    // [x_w,y_w]^T += R(yaw_w)[t_x,t_y]^T, yaw_w += Δyaw.
    const double c = std::cos(pose_.yaw);
    const double s = std::sin(pose_.yaw);
    pose_.x += c * motion.x - s * motion.y;
    pose_.y += s * motion.x + c * motion.y;
    pose_.yaw = wrap_angle(pose_.yaw + motion.yaw);

    publish_odometry(current.stamp);
    previous_ = current;
    ++update_count_;

    const auto finished_at = std::chrono::steady_clock::now();
    const auto solve_us = static_cast<long long>(
      std::chrono::duration_cast<std::chrono::microseconds>(finished_at - started_at).count());
    max_solve_us_ = std::max(max_solve_us_, solve_us);
    if (update_count_ % 20U == 0U || used_count < pair_count) {
      publish_diagnostics("tracking", pair_count, used_count, rmse, solve_us);
    }
  }

  // 표준 Odometry 메시지에 누적 SE(2) pose를 싣는다. twist는 이 예제의 학습 범위 밖이라 0으로 둔다.
  void publish_odometry(const builtin_interfaces::msg::Time & stamp)
  {
    nav_msgs::msg::Odometry message;
    message.header.stamp = stamp;
    message.header.frame_id = "odom";
    message.child_frame_id = "base_link_vo";
    message.pose.pose.position.x = pose_.x;
    message.pose.pose.position.y = pose_.y;
    message.pose.pose.orientation = yaw_to_quaternion(pose_.yaw);
    odometry_pub_->publish(message);
  }

  // 문자열 formatting과 logging은 동적 할당/lock 가능성이 있어 측정된 수학 hot path 뒤에서만 수행한다.
  void publish_diagnostics(
    const char * state, const std::size_t matched, const std::size_t inliers,
    const double rmse, const long long solve_us)
  {
    std_msgs::msg::String message;
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4)
           << "state=" << state
           << " updates=" << update_count_
           << " matched=" << matched
           << " inliers=" << inliers
           << " rmse_m=" << rmse
           << " solve_us=" << solve_us
           << " max_solve_us=" << max_solve_us_
           << " pose=(" << pose_.x << "," << pose_.y << "," << pose_.yaw << ")";
    message.data = stream.str();
    diagnostics_pub_->publish(message);
  }

  Frame previous_{};
  bool have_previous_{false};
  Transform2 pose_{};
  std::uint64_t update_count_{0U};
  long long max_solve_us_{0LL};
  rclcpp::Subscription<sensor_msgs::msg::PointCloud>::SharedPtr feature_sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odometry_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diagnostics_pub_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  const auto node = std::make_shared<daily_robotics::BoundedVoEstimator>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
