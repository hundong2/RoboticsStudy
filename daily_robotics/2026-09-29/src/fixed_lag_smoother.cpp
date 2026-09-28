#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_09_29/fixed_lag_solver.hpp"
#include "daily_robotics_2026_09_29/msg/fusion_stats.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusM = 6378137.0;
constexpr double kReferenceLatitudeDeg = 37.0;
constexpr double kReferenceLongitudeDeg = 127.0;
constexpr std::int64_t kMaximumPairSkewNs = 60000000LL;
}

using daily_robotics_2026_09_29::FixedLagSolver;
using daily_robotics_2026_09_29::Vec2;

// FixedLagSmoother의 역할:
// wheel odometry 상대 factor와 GNSS 절대 factor를 8-pose 창에서 최적화하고,
// Huber loss로 GNSS multipath 이상치를 약화한 map-frame 위치를 발행한다.
class FixedLagSmoother : public rclcpp::Node
{
public:
  FixedLagSmoother()
  : Node("fixed_lag_smoother")
  {
    const auto sensor_qos = rclcpp::SensorDataQoS().keep_last(8);
    // create_subscription의 lambda는 [this]로 노드 instance를 캡처하는 C++17 closure다.
    odom_subscription_ = create_subscription<nav_msgs::msg::Odometry>(
      "/wheel/odometry", sensor_qos,
      [this](const nav_msgs::msg::Odometry::ConstSharedPtr message) {on_odometry(message);});
    gnss_subscription_ = create_subscription<sensor_msgs::msg::NavSatFix>(
      "/gnss/fix", sensor_qos,
      [this](const sensor_msgs::msg::NavSatFix::ConstSharedPtr message) {on_gnss(message);});

    fused_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/fusion/odometry", rclcpp::QoS(10).reliable());
    stats_publisher_ = create_publisher<daily_robotics_2026_09_29::msg::FusionStats>(
      "/fusion/stats", rclcpp::QoS(10).reliable());
  }

private:
  void on_odometry(const nav_msgs::msg::Odometry::ConstSharedPtr & message)
  {
    const Vec2 raw{{message->pose.pose.position.x, message->pose.pose.position.y}};
    if (!std::isfinite(raw[0]) || !std::isfinite(raw[1])) {
      RCLCPP_WARN(get_logger(), "finite가 아닌 odometry pose를 거부합니다");
      return;
    }
    solver_.add_odometry(rclcpp::Time(message->header.stamp).nanoseconds(), raw);
    optimize_and_publish();
  }

  void on_gnss(const sensor_msgs::msg::NavSatFix::ConstSharedPtr & message)
  {
    if (message->status.status < sensor_msgs::msg::NavSatStatus::STATUS_FIX ||
      !std::isfinite(message->latitude) || !std::isfinite(message->longitude))
    {
      return;
    }

    // 위경도를 reference 주변 local ENU 평면으로 투영한다. 넓은 지역/고도에서는
    // GeographicLib 또는 robot_localization/navsat_transform_node 같은 정식 변환이 필요하다.
    const double north = (message->latitude - kReferenceLatitudeDeg) * kPi / 180.0 *
      kEarthRadiusM;
    const double east = (message->longitude - kReferenceLongitudeDeg) * kPi / 180.0 *
      kEarthRadiusM * std::cos(kReferenceLatitudeDeg * kPi / 180.0);
    const double variance_x = message->position_covariance[0];
    const double variance_y = message->position_covariance[4];
    const double sigma = std::sqrt(std::max(0.0025, 0.5 * (variance_x + variance_y)));

    if (solver_.attach_gnss(
        rclcpp::Time(message->header.stamp).nanoseconds(), Vec2{{east, north}}, sigma,
        kMaximumPairSkewNs))
    {
      optimize_and_publish();
    }
  }

  void optimize_and_publish()
  {
    const FixedLagSolver::Report report = solver_.optimize();
    if (!report.solved) {
      RCLCPP_ERROR(get_logger(), "fixed-lag normal equation 풀이가 실패했습니다");
      return;
    }

    const FixedLagSolver::State & newest = solver_.newest();
    nav_msgs::msg::Odometry fused;
    fused.header.stamp = rclcpp::Time(newest.stamp_ns);
    fused.header.frame_id = "map";
    fused.child_frame_id = "base_link";
    fused.pose.pose.position.x = newest.estimate[0];
    fused.pose.pose.position.y = newest.estimate[1];
    fused.pose.pose.orientation.w = 1.0;
    // H^-1의 최신 x/y diagonal을 계산해 Odometry covariance 위치에 기록한다.
    fused.pose.covariance[0] = report.latest_variance_x;
    fused.pose.covariance[7] = report.latest_variance_y;
    fused_publisher_->publish(fused);

    daily_robotics_2026_09_29::msg::FusionStats stats;
    stats.header = fused.header;
    stats.window_size = static_cast<std::uint8_t>(solver_.size());
    stats.irls_iterations = static_cast<std::uint8_t>(FixedLagSolver::kIrlsIterations);
    stats.marginalizations = solver_.marginalizations();
    stats.gnss_associated = solver_.gnss_associated();
    stats.gnss_rejected = solver_.gnss_rejected();
    stats.downweighted_outliers = solver_.downweighted_outliers();
    stats.minimum_gnss_weight = report.minimum_gnss_weight;
    stats.maximum_pair_skew_ms = solver_.maximum_pair_skew_ms();
    stats.latest_variance_x = report.latest_variance_x;
    stats.latest_variance_y = report.latest_variance_y;
    stats.pivot_condition_proxy = report.pivot_condition_proxy;
    stats.maximum_solve_time_us = solver_.maximum_solve_time_us();
    stats_publisher_->publish(stats);
  }

  FixedLagSolver solver_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gnss_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr fused_publisher_;
  rclcpp::Publisher<daily_robotics_2026_09_29::msg::FusionStats>::SharedPtr stats_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor를 쓰면 odometry/GNSS가 동시에 solver state를 수정하지 않는다.
  // 여러 thread가 필요하다면 MutuallyExclusive callback group 또는 explicit lock이 필요하다.
  rclcpp::spin(std::make_shared<FixedLagSmoother>());
  rclcpp::shutdown();
  return 0;
}
