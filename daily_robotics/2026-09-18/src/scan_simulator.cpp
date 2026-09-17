#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

namespace {
constexpr std::size_t kBeamCount = 181;
constexpr float kPi = 3.14159265358979323846F;
}  // namespace

// 고정 위치의 2D 라이다를 흉내 낸다. map 원점의 laser가 x=2 m, |y|<=1 m 벽을 본다.
class ScanSimulator final : public rclcpp::Node {
 public:
  ScanSimulator() : Node("scan_simulator") {
    // SensorDataQoS는 작은 keep-last 큐와 best-effort 전송을 택해 오래된 센서 데이터를 쌓지 않는다.
    publisher_ = create_publisher<sensor_msgs::msg::LaserScan>("/scan", rclcpp::SensorDataQoS());
    // wall timer는 시뮬레이션 시계 설정과 무관하게 200 ms마다 새 스캔을 만든다.
    timer_ = create_wall_timer(std::chrono::milliseconds(200),
                               [this]() { publish_scan(); });
  }

 private:
  // LaserScan의 각도·범위 계약을 채우고, 수직 벽과 광선의 교차 거리 r=2/cos(theta)를 계산한다.
  void publish_scan() {
    sensor_msgs::msg::LaserScan scan;
    // Header.stamp는 첫 광선의 취득 시각이고 frame_id는 광선 좌표계 이름이다.
    scan.header.stamp = now();
    scan.header.frame_id = "map";  // 교육용: 라이다가 정지해 있고 map과 정확히 일치한다.
    scan.angle_min = -kPi / 2.0F;
    scan.angle_max = kPi / 2.0F;
    scan.angle_increment = kPi / static_cast<float>(kBeamCount - 1);
    scan.time_increment = 0.0F;  // 정지 스캔 데모이므로 광선별 운동 보정은 하지 않는다.
    scan.scan_time = 0.2F;
    scan.range_min = 0.1F;
    scan.range_max = 4.0F;
    scan.ranges.resize(kBeamCount, scan.range_max);

    for (std::size_t i = 0; i < kBeamCount; ++i) {
      const float theta = scan.angle_min + static_cast<float>(i) * scan.angle_increment;
      const float c = std::cos(theta);
      if (c <= 0.0F) { continue; }
      // x=r*cos(theta)=2, y=r*sin(theta); 벽 구간 밖 또는 최대 범위 밖이면 free-only 광선이다.
      const float range = 2.0F / c;
      const float y = range * std::sin(theta);
      if (std::abs(y) <= 1.0F && range < scan.range_max) {
        scan.ranges[i] = std::max(scan.range_min, range);
      }
    }
    // publish()는 ROS/DDS 전송을 수행한다. 메시지 벡터 할당까지 포함하므로 이 노드는 hard RT가 아니다.
    publisher_->publish(scan);
  }

  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv) {
  // init→spin→shutdown은 ROS 2 문맥 생성, 콜백 실행, 자원 정리의 표준 수명주기다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ScanSimulator>());
  rclcpp::shutdown();
  return 0;
}
