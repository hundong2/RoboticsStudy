#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "tf2_ros/static_transform_broadcaster.h"
#include "tf2_ros/transform_broadcaster.h"

namespace {
using namespace std::chrono_literals;
constexpr std::size_t kBeamCount = 181;
constexpr double kPi = 3.14159265358979323846;
constexpr double kScanTime = 0.10;  // 첫 광선부터 마지막 광선까지 100 ms가 걸린다.
constexpr double kPublicationLag = 0.02;  // 마지막 광선 뒤 20 ms 후 메시지가 도착하는 센서 모델.
constexpr double kLaserOffsetX = 0.20;
constexpr double kWallX = 4.0;
constexpr double kWallHalfHeight = 4.5;

// Jazzy에서 rclcpp::Time을 builtin_interfaces::Time으로 명시 변환한다.
builtin_interfaces::msg::Time to_stamp(const rclcpp::Time & time) {
  builtin_interfaces::msg::Time stamp;
  const auto nanoseconds = time.nanoseconds();
  stamp.sec = static_cast<std::int32_t>(nanoseconds / 1000000000LL);
  stamp.nanosec = static_cast<std::uint32_t>(nanoseconds % 1000000000LL);
  return stamp;
}

struct Pose2d {
  double x;
  double y;
  double yaw;
};
}  // namespace

// 움직이는 base_link와 100 ms rolling LaserScan을 만든다.
// 로봇 시스템에서의 역할은 "각 광선이 서로 다른 포즈에서 취득되는 센서 입력"을 재현하는 것이다.
class MovingScanSimulator final : public rclcpp::Node {
 public:
  MovingScanSimulator() : Node("moving_scan_simulator"), epoch_(now()) {
    // SensorDataQoS는 keep-last 5 / best-effort / volatile이다. 오래된 스캔의 완전 전달보다
    // 최신 센서 지연을 우선하는 선택이며, mapper도 같은 QoS로 구독한다.
    scan_pub_ = create_publisher<sensor_msgs::msg::LaserScan>("/scan", rclcpp::SensorDataQoS());

    // TransformBroadcaster는 시간에 따라 변하는 map→base_link를 /tf에 발행한다.
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
    // StaticTransformBroadcaster는 변하지 않는 base_link→laser 외부 파라미터를 /tf_static에 보낸다.
    static_broadcaster_ = std::make_unique<tf2_ros::StaticTransformBroadcaster>(*this);
    publish_static_transform();

    // 100 Hz TF는 10 Hz 스캔의 시작·끝 시각을 TF Buffer가 보간할 재료를 제공한다.
    timer_ = create_wall_timer(10ms, [this]() { on_tick(); });
  }

 private:
  // 교육용 연속 궤적이다. 시간 t의 포즈를 직접 계산하므로 스캔 생성과 TF가 같은 기준을 공유한다.
  Pose2d pose_at(const rclcpp::Time & stamp) const {
    const double t = (stamp - epoch_).seconds();
    return Pose2d{
      1.0 + 0.20 * std::sin(0.30 * t),
      0.80 * std::sin(0.40 * t),
      0.70 * std::sin(1.10 * t)};
  }

  // yaw를 쿼터니언 q=[0,0,sin(yaw/2),cos(yaw/2)]로 바꾼다.
  // 2D 회전을 ROS의 3D Transform 메시지로 표현하는 표준 형태다.
  void set_yaw(geometry_msgs::msg::TransformStamped & transform, double yaw) const {
    transform.transform.rotation.x = 0.0;
    transform.transform.rotation.y = 0.0;
    transform.transform.rotation.z = std::sin(0.5 * yaw);
    transform.transform.rotation.w = std::cos(0.5 * yaw);
  }

  // base_link 원점에서 x=0.20 m 앞에 라이다가 있다는 고정 외부 파라미터를 한 번 발행한다.
  void publish_static_transform() {
    geometry_msgs::msg::TransformStamped transform;
    transform.header.stamp = to_stamp(now());
    transform.header.frame_id = "base_link";
    transform.child_frame_id = "laser";
    transform.transform.translation.x = kLaserOffsetX;
    set_yaw(transform, 0.0);
    static_broadcaster_->sendTransform(transform);
  }

  // 100 Hz마다 현재 포즈 TF를 먼저 발행하고, 10 tick마다 이미 끝난 과거 스캔을 발행한다.
  void on_tick() {
    const rclcpp::Time current = now();
    const Pose2d pose = pose_at(current);
    geometry_msgs::msg::TransformStamped transform;
    transform.header.stamp = to_stamp(current);
    transform.header.frame_id = "map";
    transform.child_frame_id = "base_link";
    transform.transform.translation.x = pose.x;
    transform.transform.translation.y = pose.y;
    set_yaw(transform, pose.yaw);
    tf_broadcaster_->sendTransform(transform);

    ++tick_count_;
    // 첫 0.4 s는 TF Buffer가 과거 구간을 확보하도록 기다린다.
    if (tick_count_ > 40 && tick_count_ % 10 == 0) {
      publish_scan(current);
    }
  }

  // i번째 광선 시각 t_i=stamp+i*time_increment에서 수직 벽 x=4와의 교차 거리를 계산한다.
  void publish_scan(const rclcpp::Time & current) {
    const rclcpp::Time scan_end = current - rclcpp::Duration::from_seconds(kPublicationLag);
    const rclcpp::Time scan_start = scan_end - rclcpp::Duration::from_seconds(kScanTime);

    sensor_msgs::msg::LaserScan scan;
    // LaserScan 계약상 header.stamp는 메시지 발행 시각이 아니라 첫 번째 광선 취득 시각이다.
    scan.header.stamp = to_stamp(scan_start);
    scan.header.frame_id = "laser";
    scan.angle_min = static_cast<float>(-kPi / 2.0);
    scan.angle_max = static_cast<float>(kPi / 2.0);
    scan.angle_increment = static_cast<float>(kPi / static_cast<double>(kBeamCount - 1));
    scan.time_increment = static_cast<float>(kScanTime / static_cast<double>(kBeamCount - 1));
    scan.scan_time = static_cast<float>(kScanTime);
    scan.range_min = 0.10F;
    scan.range_max = 8.0F;
    scan.ranges.resize(kBeamCount, std::numeric_limits<float>::infinity());

    for (std::size_t i = 0; i < kBeamCount; ++i) {
      // t_i = t_0 + i*Δt: deskew가 복원해야 할 실제 광선별 취득 시각이다.
      const double offset = static_cast<double>(i) * static_cast<double>(scan.time_increment);
      const rclcpp::Time beam_stamp = scan_start + rclcpp::Duration::from_seconds(offset);
      const Pose2d beam_pose = pose_at(beam_stamp);
      const double laser_x = beam_pose.x + std::cos(beam_pose.yaw) * kLaserOffsetX;
      const double laser_y = beam_pose.y + std::sin(beam_pose.yaw) * kLaserOffsetX;
      const double local_angle = static_cast<double>(scan.angle_min) +
        static_cast<double>(i) * static_cast<double>(scan.angle_increment);
      const double world_angle = beam_pose.yaw + local_angle;
      const double direction_x = std::cos(world_angle);
      if (direction_x <= 1.0e-6) {
        continue;  // 뒤를 보는 광선은 x=4 벽과 양의 거리에서 만나지 않는다.
      }
      const double ideal_range = (kWallX - laser_x) / direction_x;
      const double hit_y = laser_y + ideal_range * std::sin(world_angle);
      if (ideal_range < scan.range_min || ideal_range >= scan.range_max ||
          std::abs(hit_y) > kWallHalfHeight) {
        continue;
      }
      // 재현 가능한 ±5 mm 결정론적 잡음이다. 난수 상태가 없어 실행 간 결과가 안정적이다.
      const double noise = 0.005 * std::sin(0.37 * static_cast<double>(i) +
                                           0.11 * static_cast<double>(scan_sequence_));
      scan.ranges[i] = static_cast<float>(std::clamp(ideal_range + noise,
                                                     0.10, 7.99));
    }

    // publish()와 ranges 벡터는 동적 할당/DDS 경로를 포함하므로 hard RT라고 주장하지 않는다.
    scan_pub_->publish(scan);
    ++scan_sequence_;
  }

  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
  std::unique_ptr<tf2_ros::StaticTransformBroadcaster> static_broadcaster_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time epoch_;
  std::uint64_t tick_count_{0};
  std::uint32_t scan_sequence_{0};
};

int main(int argc, char ** argv) {
  // init→spin→shutdown은 ROS 2 context 생성, 콜백 실행, DDS/TF 자원 정리의 표준 수명주기다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MovingScanSimulator>());
  rclcpp::shutdown();
  return 0;
}
