#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "geometry_msgs/msg/point32.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/channel_float32.hpp"
#include "sensor_msgs/msg/point_cloud.hpp"

namespace daily_robotics
{

constexpr double kPi = 3.14159265358979323846;
constexpr std::size_t kLandmarkCount = 24U;

// 2차원 점을 이름 있는 구조체로 두면 배열 인덱스 0/1보다 좌표 수식의 의미가 분명해진다.
struct Point2
{
  double x{};
  double y{};
};

// yaw를 ROS quaternion으로 바꾼다. 평면 회전에서는 q=[0,0,sin(yaw/2),cos(yaw/2)]이다.
geometry_msgs::msg::Quaternion yaw_to_quaternion(const double yaw)
{
  geometry_msgs::msg::Quaternion q;
  q.z = std::sin(0.5 * yaw);
  q.w = std::cos(0.5 * yaw);
  return q;
}

// 이 노드는 동일한 landmark를 본 camera bearing과 LiDAR metric point를 만들고,
// 센서 clock skew·두 종류의 outlier를 결정론적으로 삽입해 동기화/VO의 실패 모드를 재현한다.
class SensorSimulator final : public rclcpp::Node
{
public:
  SensorSimulator()
  : Node("sensor_simulator")
  {
    // SensorDataQoS는 센서 stream에 맞춘 best-effort/작은 queue 기본값이다.
    // 동기화 subscriber도 같은 QoS profile을 써야 DDS 호환성 문제로 표본을 잃지 않는다.
    const auto sensor_qos = rclcpp::SensorDataQoS();
    camera_pub_ = create_publisher<sensor_msgs::msg::PointCloud>(
      "/camera/features", sensor_qos);
    lidar_pub_ = create_publisher<sensor_msgs::msg::PointCloud>(
      "/lidar/features", sensor_qos);

    // Auditor가 늦게 떠도 최신 기준 pose를 받을 수 있도록 depth 10의 Reliable QoS를 사용한다.
    truth_pub_ = create_publisher<nav_msgs::msg::Odometry>(
      "/sim/ground_truth", rclcpp::QoS(10).reliable());

    build_landmarks();

    using namespace std::chrono_literals;
    // wall timer는 약 33 Hz(30 ms)로 센서 frame을 만든다. 실제 장치에서는 hardware trigger가 이 역할을 한다.
    timer_ = create_wall_timer(30ms, [this]() { publish_frame(); });
  }

private:
  // 원형에 약간의 반지름 변화를 둔 landmark map을 한 번만 만들어 hot callback의 준비 작업을 줄인다.
  void build_landmarks()
  {
    for (std::size_t i = 0; i < kLandmarkCount; ++i) {
      const double angle = 2.0 * kPi * static_cast<double>(i) /
        static_cast<double>(kLandmarkCount);
      const double radius = 4.5 + 0.35 * static_cast<double>(i % 4U);
      landmarks_[i] = Point2{radius * std::cos(angle), radius * std::sin(angle)};
    }
  }

  // world landmark L을 robot/sensor frame 점 p=R(yaw)^T(L-t)로 변환한다.
  // R^T를 쓰는 이유는 world에 놓인 점을 움직이는 robot 좌표계에서 관찰하기 때문이다.
  static Point2 world_to_sensor(
    const Point2 & landmark, const double robot_x, const double robot_y, const double robot_yaw)
  {
    const double dx = landmark.x - robot_x;
    const double dy = landmark.y - robot_y;
    const double c = std::cos(robot_yaw);
    const double s = std::sin(robot_yaw);
    return Point2{c * dx + s * dy, -s * dx + c * dy};
  }

  // 센서 한 frame을 발행한다. camera와 LiDAR arrival은 같아도 Header stamp를 다르게 해
  // message_filters가 arrival time이 아니라 측정 시각을 사용한다는 점을 분리해 보여준다.
  void publish_frame()
  {
    const double t = 0.03 * static_cast<double>(frame_index_);
    // 시작 pose가 [0,0,0]이 되도록 궤적을 잡아 VO의 상대 원점과 ground truth 원점을 맞춘다.
    const double robot_x = 0.35 * std::sin(0.16 * t);
    const double robot_y = 0.25 * (1.0 - std::cos(0.12 * t));
    const double robot_yaw = 0.18 * std::sin(0.10 * t);

    const rclcpp::Time lidar_stamp = get_clock()->now();
    // 6/10/14/18 ms를 반복한다. 모두 동기화 계약 20 ms 안에 있다.
    const std::int64_t skew_ms = 6 + 4 * static_cast<std::int64_t>(frame_index_ % 4U);
    const rclcpp::Time camera_stamp = lidar_stamp -
      rclcpp::Duration::from_nanoseconds(skew_ms * 1'000'000LL);

    sensor_msgs::msg::PointCloud camera;
    camera.header.stamp = camera_stamp;
    camera.header.frame_id = "camera_optical_frame";
    camera.points.reserve(kLandmarkCount);
    camera.channels.resize(1U);
    camera.channels[0].name = "feature_id";
    camera.channels[0].values.reserve(kLandmarkCount);

    sensor_msgs::msg::PointCloud lidar;
    lidar.header.stamp = lidar_stamp;
    lidar.header.frame_id = "lidar_frame";
    lidar.points.reserve(kLandmarkCount);
    lidar.channels.resize(1U);
    lidar.channels[0].name = "feature_id";
    lidar.channels[0].values.reserve(kLandmarkCount);

    for (std::size_t i = 0; i < kLandmarkCount; ++i) {
      Point2 metric = world_to_sensor(landmarks_[i], robot_x, robot_y, robot_yaw);

      // 9 frame마다 한 correspondence를 camera와 LiDAR 양쪽에서 같은 방향으로 오염시킨다.
      // 센서 간 bearing gate는 통과하지만 frame-to-frame VO residual gate가 제거해야 하는 outlier다.
      if ((frame_index_ > 0U) && (frame_index_ % 9U == 0U) && (i == 7U)) {
        metric.x += 0.70;
        metric.y -= 0.45;
      }

      const double range = std::hypot(metric.x, metric.y);
      geometry_msgs::msg::Point32 lidar_point;
      lidar_point.x = static_cast<float>(metric.x);
      lidar_point.y = static_cast<float>(metric.y);
      lidar_point.z = 0.0F;
      lidar.points.push_back(lidar_point);

      // camera feature는 깊이를 버린 단위 bearing b=p/||p||이다.
      geometry_msgs::msg::Point32 bearing;
      bearing.x = static_cast<float>(metric.x / range);
      bearing.y = static_cast<float>(metric.y / range);
      bearing.z = 0.0F;

      // 7 frame마다 별도의 camera-only 오검출을 넣어 sync node의 cross-sensor gate를 시험한다.
      if ((frame_index_ > 0U) && (frame_index_ % 7U == 0U) && (i == 5U)) {
        bearing.x = -bearing.x;
      }
      camera.points.push_back(bearing);

      // float channel이지만 0..23 정수는 정확히 표현된다. 두 stream에서 같은 ID가 같은 landmark다.
      camera.channels[0].values.push_back(static_cast<float>(i));
      lidar.channels[0].values.push_back(static_cast<float>(i));
    }

    nav_msgs::msg::Odometry truth;
    truth.header.stamp = lidar_stamp;
    truth.header.frame_id = "odom";
    truth.child_frame_id = "base_link_truth";
    truth.pose.pose.position.x = robot_x;
    truth.pose.pose.position.y = robot_y;
    truth.pose.pose.orientation = yaw_to_quaternion(robot_yaw);

    camera_pub_->publish(camera);
    lidar_pub_->publish(lidar);
    truth_pub_->publish(truth);

    if (frame_index_ % 20U == 0U) {
      RCLCPP_INFO(
        get_logger(), "frame=%lu stamp_skew_ms=%ld truth=(%.3f, %.3f, %.3f)",
        static_cast<unsigned long>(frame_index_), static_cast<long>(skew_ms),
        robot_x, robot_y, robot_yaw);
    }
    ++frame_index_;
  }

  std::array<Point2, kLandmarkCount> landmarks_{};
  std::uint64_t frame_index_{0U};
  rclcpp::Publisher<sensor_msgs::msg::PointCloud>::SharedPtr camera_pub_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud>::SharedPtr lidar_pub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr truth_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  // init은 ROS context, signal handler, command-line remapping을 준비한다.
  rclcpp::init(argc, argv);
  const auto node = std::make_shared<daily_robotics::SensorSimulator>();
  // spin은 executor가 timer callback을 실행할 수 있도록 현재 thread에서 wait/dispatch를 반복한다.
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
