#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

namespace
{
using namespace std::chrono_literals;

constexpr double kPi = 3.14159265358979323846;
constexpr double kRoomMinX = -2.5;
constexpr double kRoomMaxX = 2.5;
constexpr double kRoomMinY = -2.0;
constexpr double kRoomMaxY = 2.0;
constexpr std::size_t kBeamCount = 16;

double normalize_angle(double angle)
{
  // atan2(sin, cos)는 어떤 각도든 [-pi, pi]로 접어 yaw 누적 오차를 안정적으로 표현한다.
  return std::atan2(std::sin(angle), std::cos(angle));
}

double ray_to_room_wall(double x, double y, double angle)
{
  // 광선 p(t)=[x,y]+t[cos(angle),sin(angle)]와 직사각형 네 벽의 양의 교점 중
  // 가장 작은 t가 이상적인 Lidar range다.
  const double direction_x = std::cos(angle);
  const double direction_y = std::sin(angle);
  double nearest = std::numeric_limits<double>::infinity();

  if (std::abs(direction_x) > 1.0e-9) {
    const double wall_x = direction_x > 0.0 ? kRoomMaxX : kRoomMinX;
    const double t = (wall_x - x) / direction_x;
    const double hit_y = y + t * direction_y;
    if (t > 0.0 && hit_y >= kRoomMinY && hit_y <= kRoomMaxY) {
      nearest = std::min(nearest, t);
    }
  }

  if (std::abs(direction_y) > 1.0e-9) {
    const double wall_y = direction_y > 0.0 ? kRoomMaxY : kRoomMinY;
    const double t = (wall_y - y) / direction_y;
    const double hit_x = x + t * direction_x;
    if (t > 0.0 && hit_x >= kRoomMinX && hit_x <= kRoomMaxX) {
      nearest = std::min(nearest, t);
    }
  }

  return nearest;
}
}  // namespace

class SensorSimulator : public rclcpp::Node
{
public:
  SensorSimulator()
  : Node("sensor_simulator")
  {
    // KeepLast(1)+Reliable은 오래된 속도 명령을 쌓지 않고 최신 명령 하나를 확실히 전달한다.
    command_publisher_ = create_publisher<geometry_msgs::msg::Twist>(
      "/cmd_vel", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());
    // SensorDataQoS는 BestEffort/짧은 queue로 최신 센서 샘플을 우선하는 표준 프로파일이다.
    scan_publisher_ = create_publisher<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SensorDataQoS());

    // 50 ms 주기(20 Hz)는 명령, 모의 운동, scan을 같은 이산 시각에 진행한다.
    timer_ = create_wall_timer(50ms, std::bind(&SensorSimulator::tick, this));
  }

private:
  void tick()
  {
    // 이 노드는 실제 로봇에서 joystick/navigation 명령과 Lidar driver 역할을 함께 모의한다.
    elapsed_seconds_ += 0.05;

    geometry_msgs::msg::Twist command;
    command.linear.x = 0.18;
    // 느리게 좌우로 굽는 각속도는 직진만 할 때보다 자세 관측가능성을 높인다.
    command.angular.z = 0.28 * std::sin(0.45 * elapsed_seconds_);
    command_publisher_->publish(command);

    // Unicycle 식: x'=v cos(theta), y'=v sin(theta), theta'=omega.
    // 실제 차동구동 바퀴식은 motor controller에서 계산하고, 여기서는 ground truth만 적분한다.
    true_x_ += command.linear.x * std::cos(true_yaw_) * 0.05;
    true_y_ += command.linear.x * std::sin(true_yaw_) * 0.05;
    true_yaw_ = normalize_angle(true_yaw_ + command.angular.z * 0.05);

    sensor_msgs::msg::LaserScan scan;
    scan.header.stamp = now();
    scan.header.frame_id = "base_scan";
    scan.angle_min = static_cast<float>(-kPi);
    scan.angle_increment = static_cast<float>(2.0 * kPi / kBeamCount);
    scan.angle_max = scan.angle_min +
      static_cast<float>((kBeamCount - 1U) * scan.angle_increment);
    scan.range_min = 0.05F;
    scan.range_max = 8.0F;
    scan.ranges.resize(kBeamCount);

    for (std::size_t beam = 0; beam < kBeamCount; ++beam) {
      const double local_angle = scan.angle_min + beam * scan.angle_increment;
      const double ideal_range = ray_to_room_wall(true_x_, true_y_, true_yaw_ + local_angle);
      // 재현 가능한 작은 사인 잡음으로 센서가 완벽하지 않은 상황을 만든다.
      const double noise = 0.006 * std::sin(17.0 * elapsed_seconds_ + 1.7 * beam);
      scan.ranges[beam] = static_cast<float>(
        std::clamp(ideal_range + noise, 0.05, 8.0));
    }
    scan_publisher_->publish(scan);

    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 2000,
      "ground truth: x=%.3f y=%.3f yaw=%.3f", true_x_, true_y_, true_yaw_);
  }

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr command_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;

  double elapsed_seconds_{0.0};
  double true_x_{-1.0};
  double true_y_{-0.6};
  double true_yaw_{0.25};
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 DDS/RMW context와 ROS 인자 처리를 초기화한다.
  rclcpp::init(argc, argv);
  // rclcpp::spin은 이 단순 모의 노드의 subscription/timer를 SingleThreadedExecutor로 처리한다.
  rclcpp::spin(std::make_shared<SensorSimulator>());
  // shutdown은 signal 처리 후 DDS 자원과 context를 정리한다.
  rclcpp::shutdown();
  return 0;
}
