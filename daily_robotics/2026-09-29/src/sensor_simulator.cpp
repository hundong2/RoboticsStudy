#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"
#include "sensor_msgs/msg/nav_sat_status.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusM = 6378137.0;
constexpr double kReferenceLatitudeDeg = 37.0;
constexpr double kReferenceLongitudeDeg = 127.0;
}

// 센서 시뮬레이터의 역할:
// 같은 물리 궤적에서 20 Hz wheel odometry, 5 Hz GNSS, 독립 감사용 ground truth를 만든다.
// GNSS에는 주기적 큰 이상치를 넣어 robust loss가 실제로 작동하는지 검증하게 한다.
class SensorSimulator : public rclcpp::Node
{
public:
  SensorSimulator()
  : Node("sensor_simulator")
  {
    // SensorDataQoS는 best effort/작은 queue를 기본으로 해 최신 센서 표본을 우선한다.
    // 실습의 simulator와 smoother가 동일 host라 손실은 거의 없지만 실제 링크에 맞춰 조정해야 한다.
    odom_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/wheel/odometry", rclcpp::SensorDataQoS().keep_last(5));
    gnss_publisher_ = create_publisher<sensor_msgs::msg::NavSatFix>(
      "/gnss/fix", rclcpp::SensorDataQoS().keep_last(5));
    truth_publisher_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      "/sim/ground_truth", rclcpp::QoS(10).reliable());

    // create_wall_timer는 ROS time이 정지해도 host steady clock 기준 50 ms마다 callback을 실행한다.
    timer_ = create_wall_timer(50ms, std::bind(&SensorSimulator::on_timer, this));
  }

private:
  // 20 Hz sensor tick 하나를 생성한다. 세 topic 모두 동일한 now() stamp를 공유하므로
  // 수신 노드는 arrival 순서와 무관하게 측정 시각으로 association할 수 있다.
  void on_timer()
  {
    ++sequence_;
    const double time_s = 0.05 * static_cast<double>(sequence_);
    const double true_x = 0.7 * time_s;
    const double true_y = 1.0 * std::sin(0.35 * time_s);
    const rclcpp::Time measurement_time = now();

    geometry_msgs::msg::PoseStamped truth;
    truth.header.stamp = measurement_time;
    truth.header.frame_id = "map";
    truth.pose.position.x = true_x;
    truth.pose.position.y = true_y;
    truth.pose.orientation.w = 1.0;
    // 감사 노드가 먼저 정답을 받을 가능성을 높이기 위해 truth를 odometry보다 먼저 publish한다.
    truth_publisher_->publish(truth);

    nav_msgs::msg::Odometry odometry;
    odometry.header.stamp = measurement_time;
    odometry.header.frame_id = "odom";
    odometry.child_frame_id = "base_link";
    // wheel slip/scale error를 흉내 낸 시간 비례 drift와 작은 주기 오차다.
    odometry.pose.pose.position.x = true_x + 0.020 * time_s + 0.010 * std::sin(0.7 * time_s);
    odometry.pose.pose.position.y = true_y - 0.015 * time_s + 0.008 * std::cos(0.5 * time_s);
    odometry.pose.pose.orientation.w = 1.0;
    // PoseWithCovariance 배열에서 [0], [7]은 각각 x, y 분산이다.
    odometry.pose.covariance[0] = 0.0036;
    odometry.pose.covariance[7] = 0.0036;
    odom_publisher_->publish(odometry);

    if ((sequence_ % 4U) != 0U) {
      return;
    }

    ++gnss_sequence_;
    double measured_x = true_x + 0.25 * std::sin(0.91 * time_s);
    double measured_y = true_y + 0.25 * std::cos(0.73 * time_s);
    // 매 6번째 fix에 약 10 m 크기의 multipath 이상치를 주입한다.
    if ((gnss_sequence_ % 6U) == 0U) {
      measured_x += 8.0;
      measured_y -= 6.0;
    }

    sensor_msgs::msg::NavSatFix fix;
    fix.header.stamp = measurement_time;
    // NavSatFix frame_id는 ENU covariance가 표현되는 GNSS 안테나 frame을 뜻한다.
    fix.header.frame_id = "gnss_link";
    fix.status.status = sensor_msgs::msg::NavSatStatus::STATUS_FIX;
    fix.status.service = sensor_msgs::msg::NavSatStatus::SERVICE_GPS;
    // 작은 지역에서는 equirectangular 근사로 local ENU [x east, y north]를 위경도로 바꾼다.
    fix.latitude = kReferenceLatitudeDeg + measured_y / kEarthRadiusM * 180.0 / kPi;
    fix.longitude = kReferenceLongitudeDeg + measured_x /
      (kEarthRadiusM * std::cos(kReferenceLatitudeDeg * kPi / 180.0)) * 180.0 / kPi;
    fix.altitude = 0.0;
    fix.position_covariance[0] = 0.16;
    fix.position_covariance[4] = 0.16;
    fix.position_covariance[8] = 1.0;
    fix.position_covariance_type =
      sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;
    gnss_publisher_->publish(fix);
  }

  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr gnss_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr truth_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::uint32_t sequence_{0U};
  std::uint32_t gnss_sequence_{0U};
};

int main(int argc, char ** argv)
{
  // init은 DDS participant와 ROS arguments를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 subscription/timer callback을 한 thread에서 순차 dispatch한다.
  rclcpp::spin(std::make_shared<SensorSimulator>());
  rclcpp::shutdown();
  return 0;
}
