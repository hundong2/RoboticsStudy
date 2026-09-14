#include <memory>

#include "daily_robotics_2026_09_15/msg/joint_space_world.hpp"
#include "geometry_msgs/msg/vector3.hpp"
#include "rclcpp/rclcpp.hpp"

namespace daily_robotics
{

// 이 노드는 실제 PlanningScene 대신 2축 로봇의 시작/목표 관절각과 충돌 금지 영역을 발행한다.
// RRT planner와 servo가 똑같은 world snapshot을 읽게 해 "계획 모델과 제어 모델의 계약"을 만든다.
class JointSpaceWorldNode final : public rclcpp::Node
{
public:
  JointSpaceWorldNode()
  : Node("joint_space_world")
  {
    // KeepLast(1)+Reliable+TransientLocal은 마지막 world를 DDS에 보관한다.
    // 따라서 planner가 늦게 시작해도 지도 전체를 다시 요청하는 Service 없이 최신 snapshot을 받는다.
    const auto world_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    world_pub_ = create_publisher<daily_robotics_2026_09_15::msg::JointSpaceWorld>(
      "/planning/joint_space_world", world_qos);

    // TransientLocal history가 late joiner에게 이 표본을 재전달하므로 정적 world는 한 번만 발행한다.
    publish_world();
  }

private:
  // planner가 탐색할 C-space(구성 공간)를 만든다. q=(q1,q2)의 한 점이 로봇 자세 하나다.
  void publish_world()
  {
    daily_robotics_2026_09_15::msg::JointSpaceWorld world;

    // now()는 이 Node의 ROS clock을 읽고 Header stamp는 이 snapshot의 기준 시각을 나타낸다.
    world.header.stamp = now();
    world.header.frame_id = "joint_space";
    world.start = {-2.40, -1.20};
    world.goal = {2.30, 1.40};

    // 원의 수와 좌표는 RRT가 직접 연결할 수 없고 우회해야 하는 재현 가능한 예제를 만든다.
    // Vector3의 (x,y,z)는 각각 (q1 중심,q2 중심,반지름)이다. 단위는 모두 rad다.
    geometry_msgs::msg::Vector3 obstacle;
    obstacle.x = -0.75;
    obstacle.y = -0.25;
    obstacle.z = 0.58;
    world.obstacles.push_back(obstacle);

    obstacle.x = 0.20;
    obstacle.y = 0.45;
    obstacle.z = 0.62;
    world.obstacles.push_back(obstacle);

    obstacle.x = 1.15;
    obstacle.y = 0.85;
    obstacle.z = 0.42;
    world.obstacles.push_back(obstacle);

    world_pub_->publish(world);
  }

  rclcpp::Publisher<daily_robotics_2026_09_15::msg::JointSpaceWorld>::SharedPtr world_pub_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  // rclcpp::init은 DDS/RMW, signal handler, ROS 인자를 초기화한다.
  rclcpp::init(argc, argv);
  // rclcpp::spin은 executor를 만들고 world timer callback을 종료 신호까지 처리한다.
  rclcpp::spin(std::make_shared<daily_robotics::JointSpaceWorldNode>());
  // shutdown은 DDS participant와 ROS 자원을 순서대로 정리한다.
  rclcpp::shutdown();
  return 0;
}
