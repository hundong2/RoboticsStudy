#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

using namespace std::chrono_literals;

namespace daily_robotics_2026_09_07
{

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr std::size_t kBeamCount = 360U;

// 하나의 원형 장애물을 로봇 좌표계(base_link)의 중심점과 반지름으로 표현한다.
struct CircleObstacle
{
  double x;
  double y;
  double radius;
};

// 빔 방향과 원이 만나는 가장 가까운 양의 거리를 계산한다.
// 수식은 |t*d - c|^2 = r^2의 이차방정식을 t에 대해 푼 것이다.
double ray_circle_distance(
  const double angle,
  const CircleObstacle & obstacle,
  const double fallback_range)
{
  const double direction_x = std::cos(angle);
  const double direction_y = std::sin(angle);
  // projection = d·c. d는 단위벡터라 판별식은 (d·c)^2-|c|^2+r^2가 된다.
  const double projection =
    direction_x * obstacle.x + direction_y * obstacle.y;
  const double center_squared =
    obstacle.x * obstacle.x + obstacle.y * obstacle.y;
  const double discriminant =
    projection * projection - center_squared + obstacle.radius * obstacle.radius;

  if (projection <= 0.0 || discriminant < 0.0) {
    return fallback_range;
  }

  const double near_intersection = projection - std::sqrt(discriminant);
  return near_intersection > 0.0 ? near_intersection : fallback_range;
}
}  // namespace

// 이 노드는 실제 2D Lidar와 base odometry를 대신한다. 결정론적인 장애물 스캔과
// 현재 속도를 게시하여 DWA가 매 실행에서 같은 후보를 비교할 수 있게 만든다.
class SensorSimulatorComponent final : public rclcpp::Node
{
public:
  // NodeOptions를 받는 생성자는 rclcpp_components가 노드를 동적으로 만들기 위한 규약이다.
  explicit SensorSimulatorComponent(const rclcpp::NodeOptions & options)
  : Node("sensor_simulator", options)
  {
    // SensorDataQoS는 작은 큐와 best-effort를 기본으로 하여 오래된 센서 샘플 적체를 피한다.
    scan_publisher_ = create_publisher<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SensorDataQoS());
    // Odometry는 DWA 동적 속도창의 중심이 되는 현재 v, omega를 전달한다.
    odometry_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/odom", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());

    // 100 ms wall timer는 10 Hz Lidar/odometry 드라이버 주기를 흉내 낸다.
    timer_ = create_wall_timer(
      100ms, std::bind(&SensorSimulatorComponent::on_timer, this));

    RCLCPP_INFO(
      get_logger(),
      "10 Hz deterministic LaserScan/Odometry started; intra-process=%s",
      options.use_intra_process_comms() ? "true" : "false");
  }

private:
  // 한 센서 tick의 LaserScan과 Odometry를 생성해 DWA에 전달한다.
  void on_timer()
  {
    constexpr float range_min = 0.05F;
    constexpr float range_max = 6.0F;
    constexpr double scan_period = 0.10;
    constexpr CircleObstacle obstacles[] = {
      {1.00, 0.05, 0.28},
      {2.10, 0.95, 0.38},
      {2.45, -0.90, 0.34},
    };

    // make_unique는 메시지 소유자를 하나로 만든다. 같은 프로세스의 단일 intra-process
    // 구독자는 이 주소의 메시지를 복사하지 않고 소유권으로 받을 수 있다.
    auto scan = std::make_unique<sensor_msgs::msg::LaserScan>();
    scan->header.stamp = now();
    scan->header.frame_id = "base_link";
    scan->angle_min = static_cast<float>(-kPi);
    scan->angle_max = static_cast<float>(kPi);
    scan->angle_increment = static_cast<float>((2.0 * kPi) / kBeamCount);
    scan->time_increment = static_cast<float>(scan_period / kBeamCount);
    scan->scan_time = static_cast<float>(scan_period);
    scan->range_min = range_min;
    scan->range_max = range_max;
    // LaserScan의 ranges는 가변 길이 ROS 필드라 여기서는 비 RT 센서 측에서 한 번 할당한다.
    scan->ranges.resize(kBeamCount, range_max);

    for (std::size_t beam = 0U; beam < kBeamCount; ++beam) {
      const double angle = -kPi + static_cast<double>(beam) * (2.0 * kPi / kBeamCount);
      double measured_range = static_cast<double>(range_max);
      for (const auto & obstacle : obstacles) {
        measured_range = std::min(
          measured_range,
          ray_circle_distance(angle, obstacle, static_cast<double>(range_max)));
      }
      // 작은 결정론적 리플을 넣되 실제 장애물보다 가까운 허위 음수 거리는 만들지 않는다.
      measured_range += 0.002 * std::sin(17.0 * angle + 0.1 * tick_count_);
      scan->ranges[beam] = static_cast<float>(std::clamp(
          measured_range, static_cast<double>(range_min), static_cast<double>(range_max)));
    }

    const void * const scan_address = scan.get();
    // publish(std::move(...)) 뒤에는 포인터를 다시 쓰면 안 된다. 소유권이 ROS로 이동했기 때문이다.
    scan_publisher_->publish(std::move(scan));

    auto odometry = std::make_unique<nav_msgs::msg::Odometry>();
    odometry->header.stamp = now();
    odometry->header.frame_id = "odom";
    odometry->child_frame_id = "base_link";
    // 현재 속도는 DWA의 reachable window 중심이다. 사인 변화로 정지/가속 중 상황을 함께 연습한다.
    odometry->twist.twist.linear.x = 0.24 + 0.04 * std::sin(0.08 * tick_count_);
    odometry->twist.twist.angular.z = 0.05 * std::sin(0.05 * tick_count_);
    odometry_publisher_->publish(std::move(odometry));

    ++tick_count_;
    if (tick_count_ % 20U == 1U) {
      // 주소는 관찰용일 뿐 correctness에 사용하지 않는다. Planner 로그와 같다면 소유권 전달 증거다.
      RCLCPP_INFO(
        get_logger(), "published scan unique_ptr=%p beams=%zu",
        scan_address, kBeamCount);
    }
  }

  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_publisher_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odometry_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::uint64_t tick_count_{0U};
};

}  // namespace daily_robotics_2026_09_07

// 이 매크로는 클래스 생성 함수를 pluginlib에 노출한다. main() 없이 component_container가 로드한다.
RCLCPP_COMPONENTS_REGISTER_NODE(
  daily_robotics_2026_09_07::SensorSimulatorComponent)
