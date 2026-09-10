#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"

namespace
{
constexpr std::size_t kSegments = 12;
constexpr double kPi = 3.14159265358979323846;

// 각도를 [-pi, pi] 범위로 접어 quaternion과 최적화 residual의 불연속을 줄인다.
double normalize_angle(double angle)
{
  return std::atan2(std::sin(angle), std::cos(angle));
}
}  // namespace

// 이 노드는 로봇이 원형 경로를 한 바퀴 돌아 출발점을 재방문했지만,
// wheel odometry 누적으로 마지막 pose가 어긋난 상황을 결정론적으로 재현한다.
class PathSimulator : public rclcpp::Node
{
public:
  PathSimulator()
  : Node("path_simulator")
  {
    // Reliable은 저주기 keyframe graph를 유실시키지 않으려는 선택이다.
    // Transient Local은 optimizer가 늦게 시작해도 publisher가 보관한 최신 Path 1개를 받게 한다.
    const auto graph_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    path_publisher_ = create_publisher<nav_msgs::msg::Path>("slam/raw_path", graph_qos);

    // wall timer는 시뮬레이션 시간을 쓰지 않는 독립 데모에서 1초마다 같은 graph를 재발행한다.
    using namespace std::chrono_literals;
    timer_ = create_wall_timer(1s, std::bind(&PathSimulator::publish_path, this));
    RCLCPP_INFO(get_logger(), "13-pose deterministic odometry loop simulator ready");
  }

private:
  // /slam/raw_path에 noisy odometry keyframe 열을 발행해 optimizer의 입력을 만든다.
  void publish_path()
  {
    nav_msgs::msg::Path path;
    // Header stamp는 subscriber가 publish->callback 시작 지연을 근사 측정하는 기준 시각이다.
    path.header.stamp = now();
    // raw odometry가 사는 연속적이지만 drift하는 좌표계임을 frame_id로 명시한다.
    path.header.frame_id = "odom";
    path.poses.reserve(kSegments + 1U);

    for (std::size_t index = 0; index <= kSegments; ++index) {
      const double progress = static_cast<double>(index) / static_cast<double>(kSegments);
      const double angle = 2.0 * kPi * progress;

      geometry_msgs::msg::PoseStamped pose;
      pose.header = path.header;

      // 이상적인 반지름 2 m 원에 진행률 비례 drift를 더한다.
      // p_odom(k) = p_true(k) + [0.30, -0.18]^T * k/12 이므로 마지막 gap이 의도적으로 남는다.
      pose.pose.position.x = 2.0 * std::cos(angle) + 0.30 * progress;
      pose.pose.position.y = 2.0 * std::sin(angle) - 0.18 * progress;
      pose.pose.position.z = 0.0;

      // 원의 접선 방향 yaw에 0.072 rad 누적 bias를 더해 회전 drift도 함께 만든다.
      const double yaw = normalize_angle(angle + 0.5 * kPi + 0.072 * progress);
      // 평면 회전 quaternion은 q=[0, 0, sin(yaw/2), cos(yaw/2)]이다.
      pose.pose.orientation.x = 0.0;
      pose.pose.orientation.y = 0.0;
      pose.pose.orientation.z = std::sin(0.5 * yaw);
      pose.pose.orientation.w = std::cos(0.5 * yaw);
      path.poses.push_back(pose);
    }

    // publish는 메시지를 DDS/RMW 계층에 넘긴다. 실제 직렬화·전송 시점은 RMW와 executor에 좌우된다.
    path_publisher_->publish(path);
  }

  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr path_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 CLI/ROS 인자를 해석하고 DDS context를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 executor가 timer의 준비 상태를 기다렸다가 publish_path 콜백을 실행하게 한다.
  rclcpp::spin(std::make_shared<PathSimulator>());
  // shutdown은 context와 middleware 자원을 정상적으로 정리한다.
  rclcpp::shutdown();
  return 0;
}
