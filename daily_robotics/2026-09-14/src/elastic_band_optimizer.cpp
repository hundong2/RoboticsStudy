#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "daily_robotics_2026_09_14/msg/obstacle_sample.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/header.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

using ObstacleSample = daily_robotics_2026_09_14::msg::ObstacleSample;

struct Point2
{
  double x{0.0};
  double y{0.0};
};

struct Obstacle2
{
  std::uint32_t id{0U};
  double x{0.0};
  double y{0.0};
  double radius{0.0};
  bool valid{false};
};

/// 전역 경로를 고정 크기 작업 공간에서 수축력·장애물 반발력으로 변형하는 지역 최적화 노드다.
class ElasticBandOptimizer final : public rclcpp::Node
{
public:
  static constexpr std::size_t kMaxKnots = 21U;
  static constexpr std::size_t kMaxObstacles = 8U;

  ElasticBandOptimizer()
  : Node("elastic_band_optimizer")
  {
    const auto path_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    path_subscription_ = create_subscription<nav_msgs::msg::Path>(
      "/planning/global_path", path_qos,
      [this](const nav_msgs::msg::Path & path) {on_path(path);});

    // ContentFilterOptions는 SQL WHERE와 비슷한 식을 DDS DataReader에 내려 callback 이전에 표본을 거른다.
    rclcpp::SubscriptionOptions obstacle_options;
    obstacle_options.content_filter_options.filter_expression = "x >= %0 AND x <= %1";
    // %0/%1은 숫자 문자열이다. map의 실제 계획 구간 [-0.1, 4.2]만 수신한다.
    obstacle_options.content_filter_options.expression_parameters = {"-0.1", "4.2"};
    obstacle_subscription_ = create_subscription<ObstacleSample>(
      "/perception/obstacles", rclcpp::QoS(rclcpp::KeepLast(32)).reliable(),
      [this](const ObstacleSample & sample) {on_obstacle(sample);}, obstacle_options);
    cft_enabled_ = obstacle_subscription_->is_cft_enabled();

    trajectory_publisher_ = create_publisher<nav_msgs::msg::Path>(
      "/planning/local_trajectory", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());
    diagnostic_publisher_ = create_publisher<std_msgs::msg::String>(
      "/planning/optimizer_diagnostics",
      rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
    heartbeat_publisher_ = create_publisher<std_msgs::msg::Header>(
      "/planning/optimizer_heartbeat", rclcpp::QoS(rclcpp::KeepLast(4)).reliable());

    // 20 Hz timer callback 하나만 hot path를 소유해 mutex 없이 고정 배열을 안전하게 재사용한다.
    timer_ = create_wall_timer(50ms, [this]() {optimize_and_publish();});
    RCLCPP_INFO(get_logger(), "DDS ContentFilteredTopic enabled=%s", cft_enabled_ ? "true" : "false");
  }

private:
  /// Path의 최대 21개 knot만 복사해 입력 크기와 계산량을 명시적으로 제한한다.
  void on_path(const nav_msgs::msg::Path & path)
  {
    knot_count_ = std::min(path.poses.size(), kMaxKnots);
    for (std::size_t i = 0; i < knot_count_; ++i) {
      reference_[i] = Point2{path.poses[i].pose.position.x, path.poses[i].pose.position.y};
    }
    path_ready_ = knot_count_ >= 2U;
  }

  /// DDS 필터 미지원 RMW에서도 같은 의미를 지키도록 callback 안의 software fallback을 남긴다.
  void on_obstacle(const ObstacleSample & sample)
  {
    ++received_samples_;
    if (!cft_enabled_ && (sample.x < -0.1 || sample.x > 4.2)) {
      ++software_rejected_samples_;
      return;
    }

    auto slot = std::find_if(
      obstacles_.begin(), obstacles_.end(),
      [&sample](const Obstacle2 & obstacle) {return obstacle.valid && obstacle.id == sample.id;});
    if (slot == obstacles_.end()) {
      slot = std::find_if(
        obstacles_.begin(), obstacles_.end(), [](const Obstacle2 & obstacle) {return !obstacle.valid;});
    }
    if (slot != obstacles_.end()) {
      *slot = Obstacle2{sample.id, sample.x, sample.y, sample.radius, true};
    }
  }

  /// 탄성 밴드 8회 반복과 시간 매개화를 수행하고 Path·진단·heartbeat를 발행한다.
  void optimize_and_publish()
  {
    if (!path_ready_) {
      return;
    }
    const auto callback_begin = std::chrono::steady_clock::now();

    // 매 tick을 같은 전역 경로에서 시작하면 센서 변화에 반응하면서도 수치 drift가 누적되지 않는다.
    for (std::size_t i = 0; i < knot_count_; ++i) {
      band_[i] = reference_[i];
    }

    constexpr double kContractionGain = 0.24;
    constexpr double kRepulsionGain = 0.075;
    constexpr double kRobotRadius = 0.20;
    constexpr double kInfluenceMargin = 0.48;

    for (std::size_t iteration = 0; iteration < 8U; ++iteration) {
      next_band_ = band_;
      for (std::size_t i = 1; i + 1U < knot_count_; ++i) {
        // F_int = k * (p[i-1] - 2p[i] + p[i+1]): 이산 2차 미분을 줄여 꺾임을 펴는 수축력이다.
        double force_x = kContractionGain *
          (band_[i - 1U].x - 2.0 * band_[i].x + band_[i + 1U].x);
        double force_y = kContractionGain *
          (band_[i - 1U].y - 2.0 * band_[i].y + band_[i + 1U].y);

        for (const auto & obstacle : obstacles_) {
          if (!obstacle.valid) {
            continue;
          }
          const double dx = band_[i].x - obstacle.x;
          const double dy = band_[i].y - obstacle.y;
          const double distance = std::max(std::hypot(dx, dy), 1.0e-6);
          const double influence = obstacle.radius + kRobotRadius + kInfluenceMargin;
          if (distance < influence) {
            // F_ext = eta*(d0-d)/d * unit(p-o): 장애물 가까이에서만 커지는 방사형 반발력이다.
            const double magnitude = kRepulsionGain * (influence - distance) / distance;
            force_x += magnitude * dx / distance;
            force_y += magnitude * dy / distance;
          }
        }
        next_band_[i].x = std::clamp(band_[i].x + force_x, 0.0, 4.0);
        next_band_[i].y = std::clamp(band_[i].y + force_y, -1.4, 1.4);
      }
      band_.swap(next_band_);
    }

    nav_msgs::msg::Path trajectory;
    trajectory.header.stamp = now();
    trajectory.header.frame_id = "map";
    trajectory.poses.reserve(knot_count_);
    rclcpp::Time sample_time = trajectory.header.stamp;
    double min_clearance = std::numeric_limits<double>::infinity();
    double smoothness_cost = 0.0;

    for (std::size_t i = 0; i < knot_count_; ++i) {
      geometry_msgs::msg::PoseStamped pose;
      pose.header.frame_id = "map";
      if (i > 0U) {
        const double segment = std::hypot(
          band_[i].x - band_[i - 1U].x, band_[i].y - band_[i - 1U].y);
        // dt_i = max(||p_i-p_{i-1}||/v_max, 50 ms): 공간 경로를 속도 제한 0.6 m/s의 시간 궤적으로 바꾼다.
        const double dt_seconds = std::max(segment / 0.6, 0.05);
        sample_time = sample_time + rclcpp::Duration::from_seconds(dt_seconds);
      }
      pose.header.stamp = sample_time;
      pose.pose.position.x = band_[i].x;
      pose.pose.position.y = band_[i].y;
      pose.pose.orientation.w = 1.0;
      trajectory.poses.push_back(pose);

      for (const auto & obstacle : obstacles_) {
        if (obstacle.valid) {
          min_clearance = std::min(
            min_clearance,
            std::hypot(band_[i].x - obstacle.x, band_[i].y - obstacle.y) - obstacle.radius);
        }
      }
      if (i > 0U && i + 1U < knot_count_) {
        const double ddx = band_[i - 1U].x - 2.0 * band_[i].x + band_[i + 1U].x;
        const double ddy = band_[i - 1U].y - 2.0 * band_[i].y + band_[i + 1U].y;
        smoothness_cost += ddx * ddx + ddy * ddy;
      }
    }
    trajectory_publisher_->publish(trajectory);

    ++optimizer_ticks_;
    // 80~89번째 tick에서 heartbeat만 0.5초 멈춰 supervisor의 trip/recovery 경로를 재현한다.
    if (optimizer_ticks_ < 80U || optimizer_ticks_ >= 90U) {
      std_msgs::msg::Header heartbeat;
      heartbeat.stamp = now();
      heartbeat.frame_id = "optimizer_alive";
      heartbeat_publisher_->publish(heartbeat);
    }

    const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - callback_begin).count();
    max_callback_us_ = std::max(max_callback_us_, static_cast<std::uint64_t>(elapsed_us));

    std_msgs::msg::String diagnostic;
    std::ostringstream text;
    text << "cft=" << (cft_enabled_ ? "true" : "false")
         << " received=" << received_samples_
         << " sw_rejected=" << software_rejected_samples_
         << " knots=" << knot_count_
         << " iterations=8 min_clearance_m=" << min_clearance
         << " smoothness_cost=" << smoothness_cost
         << " max_callback_us=" << max_callback_us_;
    diagnostic.data = text.str();
    diagnostic_publisher_->publish(diagnostic);
  }

  rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr path_subscription_;
  rclcpp::Subscription<ObstacleSample>::SharedPtr obstacle_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr trajectory_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diagnostic_publisher_;
  rclcpp::Publisher<std_msgs::msg::Header>::SharedPtr heartbeat_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::array<Point2, kMaxKnots> reference_{};
  std::array<Point2, kMaxKnots> band_{};
  std::array<Point2, kMaxKnots> next_band_{};
  std::array<Obstacle2, kMaxObstacles> obstacles_{};
  std::size_t knot_count_{0U};
  std::uint64_t received_samples_{0U};
  std::uint64_t software_rejected_samples_{0U};
  std::uint64_t optimizer_ticks_{0U};
  std::uint64_t max_callback_us_{0U};
  bool path_ready_{false};
  bool cft_enabled_{false};
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<daily_robotics::ElasticBandOptimizer>());
  rclcpp::shutdown();
  return 0;
}
