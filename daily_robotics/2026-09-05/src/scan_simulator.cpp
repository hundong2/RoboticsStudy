#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

using namespace std::chrono_literals;

/**
 * 정사각형 방의 벽 점들을 움직이는 2D 센서 좌표계로 변환해 LaserScan을 만드는 노드다.
 * 실제 라이다 없이도 scan matcher의 입력, QoS, 좌표 변환과 시간 흐름을 반복 실험할 수 있다.
 */
class ScanSimulator : public rclcpp::Node
{
public:
  ScanSimulator()
  : Node("scan_simulator")
  {
    // SensorDataQoS는 best-effort/volatile 중심의 센서 스트림용 기본값이다. 오래된 스캔보다 최신성이 중요하다.
    scan_publisher_ = this->create_publisher<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SensorDataQoS().keep_last(1));
    scan_timer_ = this->create_wall_timer(100ms, std::bind(&ScanSimulator::publish_scan, this));
  }

private:
  static constexpr std::size_t kBeamCount = 360;
  static constexpr double kPi = 3.14159265358979323846;

  /** 월드 좌표의 점 하나를 현재 센서 좌표로 바꾸고 해당 각도 beam의 최소 range에 반영한다. */
  static void rasterize_world_point(
    const double world_x,
    const double world_y,
    const double sensor_x,
    const double sensor_y,
    const double sensor_yaw,
    sensor_msgs::msg::LaserScan & scan)
  {
    const double dx = world_x - sensor_x;
    const double dy = world_y - sensor_y;

    // p_sensor = R(yaw)^T (p_world - t): 월드 점을 센서 원점과 축에서 본 좌표로 바꾼다.
    const double cosine = std::cos(sensor_yaw);
    const double sine = std::sin(sensor_yaw);
    const double local_x = cosine * dx + sine * dy;
    const double local_y = -sine * dx + cosine * dy;
    const double range = std::hypot(local_x, local_y);
    if (range < scan.range_min || range > scan.range_max) {
      return;
    }

    const double angle = std::atan2(local_y, local_x);
    auto beam = static_cast<std::size_t>(
      std::floor((angle - static_cast<double>(scan.angle_min)) / scan.angle_increment));
    // atan2가 정확히 +pi를 반환하면 마지막 beam으로 포화한다.
    beam = std::min(beam, kBeamCount - 1);
    scan.ranges[beam] = std::min(scan.ranges[beam], static_cast<float>(range));
  }

  /** 10 Hz로 센서 자세를 조금 이동시키고 네 벽의 관측을 /scan에 게시한다. */
  void publish_scan()
  {
    sensor_msgs::msg::LaserScan scan;
    scan.header.stamp = this->now();
    scan.header.frame_id = "laser";
    scan.angle_min = static_cast<float>(-kPi);
    scan.angle_increment = static_cast<float>((2.0 * kPi) / static_cast<double>(kBeamCount));
    scan.angle_max = scan.angle_min +
      static_cast<float>(kBeamCount - 1) * scan.angle_increment;
    scan.time_increment = 0.0F;
    scan.scan_time = 0.1F;
    scan.range_min = 0.05F;
    scan.range_max = 20.0F;
    scan.ranges.assign(kBeamCount, std::numeric_limits<float>::infinity());

    // 완만한 폐곡선 궤적은 매 프레임 작은 translation과 rotation을 만들어 ICP의 초기값 조건을 만족한다.
    const double sensor_x = 0.50 * std::sin(phase_);
    const double sensor_y = 0.20 * std::cos(0.7 * phase_);
    const double sensor_yaw = 0.08 * std::sin(0.5 * phase_);

    // 10 m x 8 m 방의 네 벽을 5 cm 간격의 점 집합으로 샘플링한다.
    for (int sample = 0; sample <= 200; ++sample) {
      const double x = -5.0 + 0.05 * static_cast<double>(sample);
      rasterize_world_point(x, -4.0, sensor_x, sensor_y, sensor_yaw, scan);
      rasterize_world_point(x, 4.0, sensor_x, sensor_y, sensor_yaw, scan);
    }
    for (int sample = 0; sample <= 160; ++sample) {
      const double y = -4.0 + 0.05 * static_cast<double>(sample);
      rasterize_world_point(-5.0, y, sensor_x, sensor_y, sensor_yaw, scan);
      rasterize_world_point(5.0, y, sensor_x, sensor_y, sensor_yaw, scan);
    }

    scan_publisher_->publish(scan);
    phase_ += 0.02;
  }

  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_publisher_;
  rclcpp::TimerBase::SharedPtr scan_timer_;
  double phase_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // spin은 100 ms timer가 준비될 때 publish_scan 콜백을 실행한다.
  rclcpp::spin(std::make_shared<ScanSimulator>());
  rclcpp::shutdown();
  return 0;
}
