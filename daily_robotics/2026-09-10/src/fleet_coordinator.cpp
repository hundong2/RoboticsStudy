#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

/**
 * @brief 두 robot namespace의 frontier goal을 전역 관점에서 관찰해 중복 목표 위험을 알린다.
 *
 * 이 노드는 안전 제어기가 아니라 fleet 계층의 최소 예제다. 실제 할당기는 map merge,
 * 통신 지연, 로봇별 비용, task lease와 장애 복구를 함께 다뤄야 한다.
 */
class FleetCoordinator : public rclcpp::Node
{
public:
  FleetCoordinator()
  : Node("fleet_coordinator")
  {
    minimum_separation_m_ = declare_parameter<double>("minimum_goal_separation_m", 0.75);

    // 앞에 '/'가 있는 Fully Qualified Name은 이 노드의 /fleet namespace 영향을 받지 않는다.
    // lambda capture의 0/1은 같은 callback 구현으로 어느 로봇의 메시지인지 구분하는 C++17 방식이다.
    robot_1_subscription_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/robot_1/frontier_goal", rclcpp::QoS(1).reliable(),
      [this](geometry_msgs::msg::PoseStamped::SharedPtr msg) {remember_goal(0, *msg);});
    robot_2_subscription_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/robot_2/frontier_goal", rclcpp::QoS(1).reliable(),
      [this](geometry_msgs::msg::PoseStamped::SharedPtr msg) {remember_goal(1, *msg);});

    // 상대 이름 exploration_status는 node namespace와 결합되어 /fleet/exploration_status가 된다.
    status_publisher_ = create_publisher<std_msgs::msg::String>(
      "exploration_status", rclcpp::QoS(1).reliable());
    timer_ = create_wall_timer(1s, std::bind(&FleetCoordinator::publish_status, this));
  }

private:
  struct GoalSnapshot
  {
    bool received{false};
    double x{0.0};
    double y{0.0};
  };

  /** PoseStamped에서 fleet 비교에 필요한 2차원 위치만 복사한다. */
  void remember_goal(const std::size_t robot_index, const geometry_msgs::msg::PoseStamped & msg)
  {
    goals_[robot_index].received = true;
    goals_[robot_index].x = msg.pose.position.x;
    goals_[robot_index].y = msg.pose.position.y;
  }

  /** 1 Hz로 두 목표의 거리와 중복 가능성을 사람이 읽는 상태로 발행한다. */
  void publish_status()
  {
    std_msgs::msg::String status;
    std::ostringstream stream;

    if (!goals_[0].received || !goals_[1].received) {
      stream << "WAITING goals: robot_1=" << goals_[0].received
             << " robot_2=" << goals_[1].received;
    } else {
      // 유클리드 거리 d=sqrt((x1-x2)^2+(y1-y2)^2)로 같은 frontier 접근 충돌 가능성을 근사한다.
      const double separation_m = std::hypot(
        goals_[0].x - goals_[1].x, goals_[0].y - goals_[1].y);
      const bool duplicate_risk = separation_m < minimum_separation_m_;
      stream << (duplicate_risk ? "REASSIGN_REQUIRED" : "GOALS_SEPARATED")
             << " separation_m=" << separation_m
             << " robot_1=(" << goals_[0].x << ',' << goals_[0].y << ')'
             << " robot_2=(" << goals_[1].x << ',' << goals_[1].y << ')';
    }

    status.data = stream.str();
    status_publisher_->publish(status);
    RCLCPP_INFO(get_logger(), "%s", status.data.c_str());
  }

  double minimum_separation_m_{0.75};
  std::array<GoalSnapshot, 2> goals_{};
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr robot_1_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr robot_2_subscription_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // coordinator는 두 subscription과 한 timer가 짧은 callback이므로 기본 단일 spin으로 충분하다.
  rclcpp::spin(std::make_shared<FleetCoordinator>());
  rclcpp::shutdown();
  return 0;
}
