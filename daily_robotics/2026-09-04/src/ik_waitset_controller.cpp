#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

using namespace std::chrono_literals;

struct IkSolution
{
  double shoulder_rad;
  double elbow_rad;
  double achieved_x;
  double achieved_y;
  double position_error;
};

// 평면 2R 팔의 닫힌형 역기구학을 계산한다.
// 입력 (x, y)를 관절각 (q1, q2)로 바꾸고, 순기구학으로 다시 검증한 결과를 돌려준다.
std::optional<IkSolution> solve_planar_ik(
  const double x,
  const double y,
  const double link_1,
  const double link_2,
  const bool positive_elbow_branch)
{
  const double radius_squared = x * x + y * y;

  // 코사인 법칙을 관절각 q2에 대해 정리한 식이다.
  // cos(q2) = (x^2 + y^2 - L1^2 - L2^2) / (2 L1 L2)
  const double raw_cos_q2 =
    (radius_squared - link_1 * link_1 - link_2 * link_2) / (2.0 * link_1 * link_2);

  // 부동소수점 반올림은 경계에서 1을 아주 조금 넘길 수 있으므로 작은 허용오차를 둔다.
  constexpr double reachability_tolerance = 1.0e-12;
  if (raw_cos_q2 < -1.0 - reachability_tolerance ||
    raw_cos_q2 > 1.0 + reachability_tolerance)
  {
    return std::nullopt;
  }

  // clamp는 sqrt(1-cos^2)가 음수가 되어 NaN이 생기는 수치 오차를 막는다.
  const double cos_q2 = std::clamp(raw_cos_q2, -1.0, 1.0);
  const double sin_magnitude = std::sqrt(std::max(0.0, 1.0 - cos_q2 * cos_q2));
  const double sin_q2 = positive_elbow_branch ? sin_magnitude : -sin_magnitude;

  // atan2(sin, cos)는 acos보다 부호를 보존하므로 두 IK 분기 중 하나를 명시할 수 있다.
  const double q2 = std::atan2(sin_q2, cos_q2);

  // q1 = atan2(y,x) - atan2(L2 sin(q2), L1 + L2 cos(q2))
  // 첫 항은 목표 벡터 방향, 둘째 항은 삼각형 내부의 링크 보정각이다.
  const double q1 =
    std::atan2(y, x) - std::atan2(link_2 * sin_q2, link_1 + link_2 * cos_q2);

  // 순기구학 x_hat=L1 cos(q1)+L2 cos(q1+q2),
  // y_hat=L1 sin(q1)+L2 sin(q1+q2)로 IK 해가 목표를 재현하는지 검증한다.
  const double achieved_x = link_1 * std::cos(q1) + link_2 * std::cos(q1 + q2);
  const double achieved_y = link_1 * std::sin(q1) + link_2 * std::sin(q1 + q2);
  const double error = std::hypot(achieved_x - x, achieved_y - y);

  return IkSolution{q1, q2, achieved_x, achieved_y, error};
}

int main(int argc, char ** argv)
{
  // init은 ROS 2 통신 컨텍스트와 Ctrl+C 종료 처리를 초기화한다.
  rclcpp::init(argc, argv);

  // 일반 Node를 만들되 rclcpp::spin에 넘기지 않는다. 아래 WaitSet 루프가 executor 역할을 맡는다.
  auto node = std::make_shared<rclcpp::Node>("ik_waitset_controller");

  // declare_parameter는 launch/CLI에서 링크 길이와 IK 분기를 바꿀 수 있게 하면서 기본값도 고정한다.
  const double link_1 = node->declare_parameter<double>("link_1_length", 0.5);
  const double link_2 = node->declare_parameter<double>("link_2_length", 0.4);
  const bool positive_elbow_branch =
    node->declare_parameter<bool>("positive_elbow_branch", true);

  if (link_1 <= 0.0 || link_2 <= 0.0) {
    RCLCPP_FATAL(node->get_logger(), "link lengths must be positive");
    rclcpp::shutdown();
    return 1;
  }

  // KeepLast(1)은 처리 지연 시 오래된 목표를 버리는 최신값 제어 패턴이다.
  // publisher와 동일하게 reliable/volatile을 사용해 QoS 비호환을 피한다.
  const auto target_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().durability_volatile();

  // create_subscription은 API상 콜백을 요구한다. 하지만 이 노드는 executor로 spin하지 않으므로
  // 콜백은 실행되지 않고, 아래 subscription->take()가 메시지를 명시적으로 꺼낸다.
  auto target_subscription =
    node->create_subscription<geometry_msgs::msg::PointStamped>(
    "/arm/target",
    target_qos,
    [](geometry_msgs::msg::PointStamped::ConstSharedPtr) {});

  // /joint_states는 robot_state_publisher가 URDF의 가동 관절을 TF로 변환할 때 소비한다.
  auto joint_publisher = node->create_publisher<sensor_msgs::msg::JointState>(
    "/joint_states", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().durability_volatile());

  // 순기구학으로 되계산한 실제 도달점을 별도 토픽에 내보내 테스트와 시각화를 쉽게 한다.
  auto achieved_publisher = node->create_publisher<geometry_msgs::msg::PointStamped>(
    "/arm/achieved_target", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());

  // 반복 루프에서 vector를 새로 만들지 않도록 JointState의 가변 배열 크기를 미리 정한다.
  // ROS 직렬화/RMW 내부 할당까지 완전히 없앤 것은 아니므로 hard RT 보장을 뜻하지는 않는다.
  sensor_msgs::msg::JointState joint_state;
  joint_state.name = {"shoulder_joint", "elbow_joint"};
  joint_state.position.resize(2);

  // WaitSet은 subscription/timer/service 등 여러 ROS 엔티티의 readiness를 직접 기다리는 API다.
  // executor의 자동 콜백 선택을 우회해 처리 순서와 trigger 조건을 애플리케이션이 정할 수 있다.
  rclcpp::WaitSet wait_set;
  wait_set.add_subscription(target_subscription);

  RCLCPP_INFO(
    node->get_logger(),
    "WaitSet IK ready: L1=%.3f, L2=%.3f, positive_elbow_branch=%s",
    link_1, link_2, positive_elbow_branch ? "true" : "false");

  while (rclcpp::ok()) {
    // wait는 DDS 데이터 도착이나 1초 timeout까지 현재 스레드를 재운다.
    // steady_clock 기반 timeout이라 시스템 시각 보정의 영향을 받지 않는다.
    const auto wait_result = wait_set.wait(1s);

    if (wait_result.kind() == rclcpp::WaitResultKind::Timeout) {
      RCLCPP_WARN(node->get_logger(), "1 s 동안 새 IK 목표가 없습니다");
      continue;
    }
    if (wait_result.kind() != rclcpp::WaitResultKind::Ready) {
      continue;
    }

    // 내부 rcl_wait_set의 첫 subscription 포인터가 null이 아니면 DDS가 읽을 데이터를 알렸다.
    // 이 예제는 등록 순서가 고정된 단일 subscription이므로 index 0을 안전하게 사용한다.
    const auto & rcl_wait_set = wait_result.get_wait_set().get_rcl_wait_set();
    if (rcl_wait_set.subscriptions[0U] == nullptr) {
      continue;
    }

    geometry_msgs::msg::PointStamped target;
    rclcpp::MessageInfo message_info;

    // take는 callback dispatch 없이 middleware 큐에서 메시지 하나를 사용자가 직접 꺼낸다.
    // false면 readiness 확인과 take 사이에 가져올 데이터가 없어진 것이므로 다음 주기를 기다린다.
    if (!target_subscription->take(target, message_info)) {
      continue;
    }

    if (target.header.frame_id != "base_link") {
      RCLCPP_ERROR(
        node->get_logger(), "지원하지 않는 frame_id='%s' (base_link 필요)",
        target.header.frame_id.c_str());
      continue;
    }

    const auto solution = solve_planar_ik(
      target.point.x, target.point.y, link_1, link_2, positive_elbow_branch);
    if (!solution.has_value()) {
      RCLCPP_WARN(
        node->get_logger(), "도달 불가 목표: x=%.3f, y=%.3f",
        target.point.x, target.point.y);
      continue;
    }

    // JointState의 name-position 인덱스는 URDF joint 이름과 정확히 일치해야 한다.
    joint_state.header.stamp = node->now();
    joint_state.position[0] = solution->shoulder_rad;
    joint_state.position[1] = solution->elbow_rad;
    joint_publisher->publish(joint_state);

    geometry_msgs::msg::PointStamped achieved;
    achieved.header.stamp = joint_state.header.stamp;
    achieved.header.frame_id = "base_link";
    achieved.point.x = solution->achieved_x;
    achieved.point.y = solution->achieved_y;
    achieved.point.z = 0.10;
    achieved_publisher->publish(achieved);

    RCLCPP_INFO(
      node->get_logger(), "q1=%.4f rad, q2=%.4f rad, FK error=%.3e m",
      solution->shoulder_rad, solution->elbow_rad, solution->position_error);
  }

  // WaitSet과 subscription은 스코프 종료 때 RAII로 정리되고, shutdown이 ROS 컨텍스트를 닫는다.
  rclcpp::shutdown();
  return 0;
}
