#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

#include "daily_robotics_2026_11_10/msg/ndt_truth.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/point_field.hpp"

namespace
{

constexpr std::size_t kReferencePointCount = 192U;
constexpr std::size_t kScanPointCount = 96U;
constexpr std::size_t kPointStep = 12U;
constexpr double kPi = 3.14159265358979323846;

struct Point2
{
  double x{};
  double y{};
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

}  // namespace

/**
 * 이 노드는 고정된 2D reference point cloud와, 그 지도를 움직이는 lidar 좌표계에서 본
 * current scan을 만든다. 실제 시스템의 map server와 LiDAR driver를 작은 합성 예제로 대체한다.
 */
class PointcloudPairSource : public rclcpp::Node
{
public:
  PointcloudPairSource()
  : Node("pointcloud_pair_source")
  {
    build_reference_points();

    // map은 변경되지 않는 상태 snapshot이다. reliable+transient_local은 늦게 시작한 matcher도
    // 마지막 map 한 장을 받을 수 있게 하며, ROS 1의 latched publisher와 비슷한 역할을 한다.
    map_publisher_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      "/ndt/reference", rclcpp::QoS(1).reliable().transient_local());

    // scan은 최신성이 중요한 센서 stream이므로 best-effort/작은 queue의 SensorDataQoS를 쓴다.
    scan_publisher_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      "/ndt/scan", rclcpp::SensorDataQoS().keep_last(1));
    truth_publisher_ = create_publisher<daily_robotics_2026_11_10::msg::NdtTruth>(
      "/ndt/truth", rclcpp::QoS(20).reliable());

    // ROS timer는 100 ms마다 새 관측을 만든다. 합성 source는 비 RT 영역이며, 실제 driver의
    // hardware timestamp를 흉내 내기 위해 한 tick에서 scan과 truth에 같은 stamp를 넣는다.
    timer_ = create_wall_timer(
      std::chrono::milliseconds(100), std::bind(&PointcloudPairSource::on_timer, this));

    publish_reference_map();
    RCLCPP_INFO(get_logger(), "Published %zu-point durable NDT reference map", kReferencePointCount);
  }

private:
  // 서로 평행하지 않은 벽 두 개와 비스듬한 구조물을 만들어 x/y/yaw가 모두 관측되게 한다.
  void build_reference_points()
  {
    std::size_t cursor = 0U;
    for (std::size_t i = 0U; i < 81U; ++i) {
      // x=-1.85는 0.4 m cell의 경계가 아닌 내부다. 경계에 벽을 두면 아주 작은 pose 오차도
      // 이웃의 빈 cell로 넘어가 NDT 학습 장면 자체가 불필요하게 불연속이 된다.
      reference_points_[cursor++] = Point2{-1.85, -2.4 + 0.06 * static_cast<double>(i)};
    }
    for (std::size_t i = 0U; i < 71U; ++i) {
      reference_points_[cursor++] = Point2{-1.8 + 0.06 * static_cast<double>(i), -1.45};
    }
    for (std::size_t i = 0U; i < 40U; ++i) {
      const double x = 0.55 + 0.045 * static_cast<double>(i);
      reference_points_[cursor++] = Point2{x, 0.35 + 0.55 * x};
    }
  }

  // PointCloud2는 점 배열 자체가 아니라 fields가 binary data의 layout을 설명하는 메시지다.
  sensor_msgs::msg::PointCloud2 make_cloud(
    const std::array<Point2, kReferencePointCount> & points,
    std::size_t point_count,
    const std::string & frame_id,
    const rclcpp::Time & stamp) const
  {
    sensor_msgs::msg::PointCloud2 cloud;
    cloud.header.stamp = stamp;
    cloud.header.frame_id = frame_id;
    cloud.height = 1U;  // height=1은 organized image가 아닌 unordered point cloud라는 뜻이다.
    cloud.width = static_cast<std::uint32_t>(point_count);
    cloud.is_bigendian = false;
    cloud.is_dense = true;
    cloud.point_step = static_cast<std::uint32_t>(kPointStep);
    cloud.row_step = static_cast<std::uint32_t>(point_count * kPointStep);

    cloud.fields.resize(3U);
    const std::array<std::string, 3U> names{"x", "y", "z"};
    for (std::size_t i = 0U; i < names.size(); ++i) {
      cloud.fields[i].name = names[i];
      cloud.fields[i].offset = static_cast<std::uint32_t>(i * sizeof(float));
      // FLOAT32는 실제 LiDAR에서 흔한 xyz 표현이며, count=1은 각 필드가 scalar라는 뜻이다.
      cloud.fields[i].datatype = sensor_msgs::msg::PointField::FLOAT32;
      cloud.fields[i].count = 1U;
    }

    // PointCloud2::data는 가변 길이 ROS 경계 버퍼다. source는 비 RT이므로 여기서 할당하되,
    // matcher는 이를 그대로 hot path에 들이지 않고 최대 96점을 고정 배열로 복사한다.
    cloud.data.resize(point_count * kPointStep);
    for (std::size_t i = 0U; i < point_count; ++i) {
      const float x = static_cast<float>(points[i].x);
      const float y = static_cast<float>(points[i].y);
      const float z = 0.0F;
      const std::size_t offset = i * kPointStep;
      std::memcpy(cloud.data.data() + offset, &x, sizeof(float));
      std::memcpy(cloud.data.data() + offset + sizeof(float), &y, sizeof(float));
      std::memcpy(cloud.data.data() + offset + 2U * sizeof(float), &z, sizeof(float));
    }
    return cloud;
  }

  void publish_reference_map()
  {
    const auto cloud = make_cloud(reference_points_, reference_points_.size(), "map", now());
    map_publisher_->publish(cloud);
  }

  // map 점 p_m을 lidar 점 p_l로 바꾸는 식은 p_l = R(theta)^T (p_m - t)다.
  // matcher는 반대 방향인 p_m = R(theta) p_l + t를 추정한다.
  void on_timer()
  {
    const double phase = 0.045 * static_cast<double>(sequence_);
    const double true_x = 0.14 * std::sin(phase);
    const double true_y = 0.10 * std::cos(0.8 * phase);
    const double true_yaw = normalize_angle(0.055 * std::sin(0.6 * phase));
    const double c = std::cos(true_yaw);
    const double s = std::sin(true_yaw);

    std::array<Point2, kReferencePointCount> scan_storage{};
    for (std::size_t i = 0U; i < kScanPointCount; ++i) {
      // reference에서 균일하게 96점을 뽑아 bounded input을 만든다.
      const Point2 & map_point = reference_points_[2U * i];
      const double dx = map_point.x - true_x;
      const double dy = map_point.y - true_y;
      const double deterministic_noise =
        0.0015 * std::sin(0.17 * static_cast<double>(sequence_) + static_cast<double>(i));
      scan_storage[i].x = c * dx + s * dy + deterministic_noise;
      scan_storage[i].y = -s * dx + c * dy - 0.5 * deterministic_noise;
    }

    const rclcpp::Time acquisition_stamp = now();
    daily_robotics_2026_11_10::msg::NdtTruth truth;
    truth.header.stamp = acquisition_stamp;
    truth.header.frame_id = "map";
    truth.sequence = sequence_;
    truth.true_x = true_x;
    truth.true_y = true_y;
    truth.true_yaw = true_yaw;

    // truth를 먼저 publish해 독립 auditor가 같은 stamp의 matcher 결과를 받을 준비를 하게 한다.
    truth_publisher_->publish(truth);
    scan_publisher_->publish(make_cloud(scan_storage, kScanPointCount, "lidar", acquisition_stamp));
    ++sequence_;
  }

  std::array<Point2, kReferencePointCount> reference_points_{};
  std::uint32_t sequence_{0U};
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr map_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr scan_publisher_;
  rclcpp::Publisher<daily_robotics_2026_11_10::msg::NdtTruth>::SharedPtr truth_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // spin은 executor가 timer와 DDS subscription callback을 꺼내 순서대로 실행하게 한다.
  rclcpp::spin(std::make_shared<PointcloudPairSource>());
  rclcpp::shutdown();
  return 0;
}
