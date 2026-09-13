#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>

#include "daily_robotics_2026_09_14/msg/obstacle_sample.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

using ObstacleSample = daily_robotics_2026_09_14::msg::ObstacleSample;

/// 전역 경로와 여러 장애물을 만들어 실제 센서·global planner 대신 쓰는 결정론적 실습 노드다.
class WorldSimulator final : public rclcpp::Node
{
public:
  WorldSimulator()
  : Node("world_simulator")
  {
    // Transient Local은 늦게 켜진 optimizer도 마지막 전역 경로를 즉시 받게 하는 DDS 내구성 정책이다.
    const auto path_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    path_publisher_ = create_publisher<nav_msgs::msg::Path>("/planning/global_path", path_qos);

    // 센서 표본은 유실 없는 실습 계측을 위해 Reliable/KeepLast(32)로 보낸다.
    obstacle_publisher_ = create_publisher<ObstacleSample>(
      "/perception/obstacles", rclcpp::QoS(rclcpp::KeepLast(32)).reliable());

    // 50 ms = 20 Hz. 장애물 스트림은 planner의 20 Hz hot path와 같은 주기로 갱신된다.
    timer_ = create_wall_timer(50ms, [this]() {publish_cycle();});
    publish_global_path();
  }

private:
  /// 시작/목표를 잇는 17개 pose의 완만한 우회 경로를 발행한다.
  void publish_global_path()
  {
    nav_msgs::msg::Path path;
    path.header.stamp = now();
    path.header.frame_id = "map";
    path.poses.reserve(17);

    for (std::size_t i = 0; i < 17; ++i) {
      geometry_msgs::msg::PoseStamped pose;
      pose.header = path.header;
      const double ratio = static_cast<double>(i) / 16.0;
      pose.pose.position.x = 4.0 * ratio;
      // y = 0.62 sin(pi*x/4)는 중앙 장애물을 위로 우회하는 초기 global path를 뜻한다.
      pose.pose.position.y = 0.62 * std::sin(std::numbers::pi * ratio);
      pose.pose.orientation.w = 1.0;
      path.poses.push_back(pose);
    }
    path_publisher_->publish(path);
  }

  /// 관심 구간 안 2개와 밖 2개를 함께 보내 DDS 필터가 callback 부하를 줄이는지 관찰하게 한다.
  void publish_cycle()
  {
    const double phase = 0.04 * static_cast<double>(cycle_);
    const std::array<ObstacleSample, 4> samples = {
      make_obstacle(1U, 1.65, 0.10 + 0.08 * std::sin(phase), 0.22),
      make_obstacle(2U, 2.85, -0.18, 0.20),
      make_obstacle(101U, -4.0, 0.0, 0.50),
      make_obstacle(102U, 8.0, 0.0, 0.50)};

    for (const auto & sample : samples) {
      obstacle_publisher_->publish(sample);
    }
    ++cycle_;

    // Transient Local 메시지는 이미 보존되지만 stamp 갱신 관찰을 위해 2초마다 재발행한다.
    if ((cycle_ % 40U) == 0U) {
      publish_global_path();
    }
  }

  /// 반복 필드 채우기를 한곳에 모아 네 장애물 메시지의 frame/stamp 계약을 동일하게 유지한다.
  ObstacleSample make_obstacle(
    const std::uint32_t id, const double x, const double y, const double radius)
  {
    ObstacleSample sample;
    sample.header.stamp = now();
    sample.header.frame_id = "map";
    sample.id = id;
    sample.x = x;
    sample.y = y;
    sample.radius = radius;
    return sample;
  }

  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr path_publisher_;
  rclcpp::Publisher<ObstacleSample>::SharedPtr obstacle_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::uint64_t cycle_{0U};
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  // rclcpp::init은 ROS 인수 파싱, context, DDS 통신 계층을 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 subscription/timer가 실행 가능한지 기다렸다가 callback을 호출하는 executor 진입점이다.
  rclcpp::spin(std::make_shared<daily_robotics::WorldSimulator>());
  rclcpp::shutdown();
  return 0;
}
