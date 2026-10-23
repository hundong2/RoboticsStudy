#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr double kDt = 0.005;          // 200 Hz IMU 주기 [s]
constexpr std::uint32_t kVisualStride = 10U;  // 10 IMU마다 20 Hz keyframe
constexpr double kTrueGyroBias = 0.015;       // 시뮬레이터가 숨겨 둔 z축 gyro bias [rad/s]

double wrap_angle(double angle)
{
  // atan2(sin, cos)는 임의의 각도를 [-pi, pi]로 접어 yaw 잔차의 2pi 점프를 막는다.
  return std::atan2(std::sin(angle), std::cos(angle));
}

void set_yaw(double yaw, geometry_msgs::msg::Quaternion & q)
{
  // 평면 회전 q=[0,0,sin(yaw/2),cos(yaw/2)]를 ROS 필드 순서 (x,y,z,w)에 기록한다.
  q.x = 0.0;
  q.y = 0.0;
  q.z = std::sin(0.5 * yaw);
  q.w = std::cos(0.5 * yaw);
}
}  // namespace

class VioSensorSimulator : public rclcpp::Node
{
public:
  VioSensorSimulator()
  : Node("vio_sensor_simulator")
  {
    // SensorDataQoS는 센서 스트림에서 오래된 backlog보다 최신 샘플을 우선하는 best-effort 프로필이다.
    // 오늘 estimator는 drop을 진단하므로, 전송 손실이 허용되는 센서 경계를 의도적으로 드러낸다.
    imu_publisher_ = create_publisher<sensor_msgs::msg::Imu>("/sim/imu", rclcpp::SensorDataQoS());

    // visual keyframe과 truth는 각 샘플이 최적화/audit에 필요하므로 Reliable KeepLast를 선택한다.
    const auto keyframe_qos = rclcpp::QoS(rclcpp::KeepLast(20)).reliable();
    visual_publisher_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/sim/visual_pose", keyframe_qos);
    truth_publisher_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      "/sim/truth_pose", keyframe_qos);

    // WallTimer는 ROS time 정지와 무관하게 합성 센서를 200 Hz로 진행시킨다.
    timer_ = create_wall_timer(5ms, std::bind(&VioSensorSimulator::tick, this));
  }

private:
  void tick()
  {
    const auto stamp = now();
    const double t = static_cast<double>(tick_count_) * kDt;

    // 물체 좌표계(body)의 가속도와 yaw rate를 부드럽게 변화시켜 회전-병진 결합을 만든다.
    const double body_ax = 0.22 + 0.08 * std::sin(0.70 * t);
    const double body_ay = 0.10 * std::sin(0.43 * t);
    const double yaw_rate = 0.22 + 0.05 * std::sin(0.31 * t);

    const double c = std::cos(yaw_);
    const double s = std::sin(yaw_);
    // a_world = R(yaw) * a_body: IMU body-frame 가속도를 지도 좌표계 운동에 적용한다.
    const double world_ax = c * body_ax - s * body_ay;
    const double world_ay = s * body_ax + c * body_ay;

    // p_{k+1}=p_k+v_k*dt+0.5*a*dt^2, v_{k+1}=v_k+a*dt의 이산 운동학이다.
    x_ += vx_ * kDt + 0.5 * world_ax * kDt * kDt;
    y_ += vy_ * kDt + 0.5 * world_ay * kDt * kDt;
    vx_ += world_ax * kDt;
    vy_ += world_ay * kDt;
    yaw_ = wrap_angle(yaw_ + yaw_rate * kDt);

    sensor_msgs::msg::Imu imu;
    imu.header.stamp = stamp;
    imu.header.frame_id = "imu_link";

    // orientation을 제공하지 않는 IMU는 covariance[0]=-1로 명시한다. 0 quaternion을 유효 자세로 읽으면 안 된다.
    imu.orientation_covariance[0] = -1.0;
    imu.angular_velocity.z = yaw_rate + kTrueGyroBias + 0.0015 * std::sin(2.7 * t);
    imu.angular_velocity_covariance = std::array<double, 9>{
      0.0, 0.0, 0.0,
      0.0, 0.0, 0.0,
      0.0, 0.0, 2.25e-6};

    // sensor_msgs/Imu의 linear_acceleration 단위는 m/s^2이고 이 실습에서는 중력 제거된 body x/y를 쓴다.
    imu.linear_acceleration.x = body_ax + 0.010 * std::sin(3.1 * t);
    imu.linear_acceleration.y = body_ay + 0.010 * std::cos(2.3 * t);
    imu.linear_acceleration.z = 0.0;
    imu.linear_acceleration_covariance = std::array<double, 9>{
      1.0e-4, 0.0, 0.0,
      0.0, 1.0e-4, 0.0,
      0.0, 0.0, 1.0e-4};
    imu_publisher_->publish(imu);

    ++tick_count_;
    if ((tick_count_ % kVisualStride) != 0U) {
      return;
    }

    // auditor용 truth와 estimator용 noisy visual pose는 같은 측정 timestamp를 공유한다.
    geometry_msgs::msg::PoseStamped truth;
    truth.header.stamp = stamp;
    truth.header.frame_id = "map";
    truth.pose.position.x = x_;
    truth.pose.position.y = y_;
    set_yaw(yaw_, truth.pose.orientation);
    truth_publisher_->publish(truth);

    geometry_msgs::msg::PoseWithCovarianceStamped visual;
    visual.header = truth.header;
    const double keyframe = static_cast<double>(tick_count_ / kVisualStride);
    visual.pose.pose.position.x = x_ + 0.012 * std::sin(0.37 * keyframe);
    visual.pose.pose.position.y = y_ + 0.012 * std::cos(0.29 * keyframe);
    set_yaw(
      wrap_angle(yaw_ + 0.006 * std::sin(0.23 * keyframe)),
      visual.pose.pose.orientation);

    // PoseWithCovariance 순서는 [x,y,z,roll,pitch,yaw]이고 row-major diagonal은 0,7,...,35다.
    visual.pose.covariance[0] = 4.0e-4;
    visual.pose.covariance[7] = 4.0e-4;
    visual.pose.covariance[14] = 1.0;
    visual.pose.covariance[21] = 1.0;
    visual.pose.covariance[28] = 1.0;
    visual.pose.covariance[35] = 1.0e-4;
    visual_publisher_->publish(visual);
  }

  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr visual_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr truth_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;

  std::uint32_t tick_count_{0U};
  double x_{0.0};
  double y_{0.0};
  double vx_{0.0};
  double vy_{0.0};
  double yaw_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<VioSensorSimulator>());
  rclcpp::shutdown();
  return 0;
}
