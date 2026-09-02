#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2/LinearMath/Quaternion.h"
#include "tf2_ros/static_transform_broadcaster.h"
#include "tf2_ros/transform_broadcaster.h"

using namespace std::chrono_literals;

namespace daily_robotics
{

/// map→base_link의 동적 자세와 base_link→lidar의 고정 외부 파라미터를 TF로 내보내고,
/// LiDAR 좌표계의 목표점을 발행하여 뒤쪽 planner가 좌표 변환을 연습하게 하는 노드다.
class TfGoalSource final : public rclcpp::Node
{
public:
  TfGoalSource()
  : Node("tf_goal_source"),
    // TransformBroadcaster는 시간에 따라 변하는 좌표 관계를 표준 /tf Topic으로 발행한다.
    dynamic_broadcaster_(this),
    // StaticTransformBroadcaster는 변하지 않는 센서 장착 위치를 /tf_static에 transient-local로 보존한다.
    static_broadcaster_(this)
  {
    // ROS 파라미터로 목표점의 LiDAR 기준 좌표를 바꿀 수 있게 해 재컴파일 없이 실험한다.
    goal_x_in_lidar_ = declare_parameter<double>("goal_x_in_lidar", 11.0);
    goal_y_in_lidar_ = declare_parameter<double>("goal_y_in_lidar", 5.0);

    // 센서성 최신 데이터이므로 작은 depth와 best-effort를 쓰는 SensorDataQoS를 선택한다.
    goal_publisher_ = create_publisher<geometry_msgs::msg::PointStamped>(
      "/planning/goal_in_lidar", rclcpp::SensorDataQoS().keep_last(1));

    send_static_sensor_transform();

    // create_wall_timer는 벽시계 기준 주기 콜백이다. 10 Hz는 TF 시각화/실습에 충분하다.
    transform_timer_ = create_wall_timer(
      100ms, std::bind(&TfGoalSource::publish_dynamic_transform, this));
    goal_timer_ = create_wall_timer(
      1s, std::bind(&TfGoalSource::publish_goal, this));

    RCLCPP_INFO(
      get_logger(), "TF source ready: goal in lidar = (%.2f, %.2f)",
      goal_x_in_lidar_, goal_y_in_lidar_);
  }

private:
  /// 로봇 본체에서 LiDAR 중심까지의 변하지 않는 외부 파라미터(extrinsic)를 한 번 발행한다.
  void send_static_sensor_transform()
  {
    geometry_msgs::msg::TransformStamped transform;
    transform.header.stamp = get_clock()->now();
    transform.header.frame_id = "base_link";
    transform.child_frame_id = "lidar";
    transform.transform.translation.x = 0.25;
    transform.transform.translation.y = 0.0;
    transform.transform.translation.z = 0.20;

    // 단위 쿼터니언 (x,y,z,w)=(0,0,0,1)은 회전이 없음을 뜻한다.
    transform.transform.rotation.x = 0.0;
    transform.transform.rotation.y = 0.0;
    transform.transform.rotation.z = 0.0;
    transform.transform.rotation.w = 1.0;

    // 정적 TF는 한 번 보내도 늦게 시작한 listener가 /tf_static에서 다시 받을 수 있다.
    static_broadcaster_.sendTransform(transform);
  }

  /// 시뮬레이션 로봇의 map 기준 위치와 yaw를 계산해 map→base_link 동적 TF로 발행한다.
  void publish_dynamic_transform()
  {
    phase_rad_ += 0.02;

    geometry_msgs::msg::TransformStamped transform;
    transform.header.stamp = get_clock()->now();
    transform.header.frame_id = "map";
    transform.child_frame_id = "base_link";

    // 작은 원운동 x=-5+0.5cos(t), y=-3+0.5sin(t)을 만들어 TF가 시간에 따라 변하게 한다.
    transform.transform.translation.x = -5.0 + 0.5 * std::cos(phase_rad_);
    transform.transform.translation.y = -3.0 + 0.5 * std::sin(phase_rad_);
    transform.transform.translation.z = 0.0;

    const double yaw_rad = 0.10 * std::sin(phase_rad_);
    tf2::Quaternion orientation;
    // setRPY(roll,pitch,yaw)는 오일러 각을 특이점에 강한 단위 쿼터니언으로 바꾼다.
    orientation.setRPY(0.0, 0.0, yaw_rad);
    orientation.normalize();
    transform.transform.rotation.x = orientation.x();
    transform.transform.rotation.y = orientation.y();
    transform.transform.rotation.z = orientation.z();
    transform.transform.rotation.w = orientation.w();

    dynamic_broadcaster_.sendTransform(transform);
  }

  /// LiDAR가 관측했다고 가정한 목표점을 PointStamped로 발행한다.
  void publish_goal()
  {
    geometry_msgs::msg::PointStamped goal;
    goal.header.frame_id = "lidar";

    // stamp=0은 TF2에서 "버퍼에 있는 가장 최신 변환"을 요청하는 관례다.
    // 센서 융합 실무에서는 실제 측정 timestamp를 넣고 MessageFilter로 변환 도착을 기다려야 한다.
    goal.header.stamp.sec = 0;
    goal.header.stamp.nanosec = 0;
    goal.point.x = goal_x_in_lidar_;
    goal.point.y = goal_y_in_lidar_;
    goal.point.z = 0.0;

    goal_publisher_->publish(goal);
  }

  tf2_ros::TransformBroadcaster dynamic_broadcaster_;
  tf2_ros::StaticTransformBroadcaster static_broadcaster_;
  rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr goal_publisher_;
  rclcpp::TimerBase::SharedPtr transform_timer_;
  rclcpp::TimerBase::SharedPtr goal_timer_;
  double goal_x_in_lidar_{11.0};
  double goal_y_in_lidar_{5.0};
  double phase_rad_{0.0};
};

}  // namespace daily_robotics

int main(int argc, char * argv[])
{
  // rclcpp::init은 DDS/RMW와 ROS 인자 처리 등 프로세스 단위 ROS 2 런타임을 초기화한다.
  rclcpp::init(argc, argv);

  // spin은 타이머와 구독 콜백이 실행될 때까지 executor event loop를 유지한다.
  rclcpp::spin(std::make_shared<daily_robotics::TfGoalSource>());

  // shutdown은 DDS participant와 ROS 자원을 정상 해제한다.
  rclcpp::shutdown();
  return 0;
}
