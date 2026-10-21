#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "daily_robotics_2026_10_22/msg/pose_graph_input.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"

namespace
{
constexpr std::size_t kPoseCount = 12U;
constexpr std::size_t kLoopCount = 3U;
constexpr double kPi = 3.14159265358979323846;

struct Pose2
{
  double x{};
  double y{};
  double yaw{};
};

// 각도를 [-pi, pi]로 감싼다. pose graph에서 +pi와 -pi는 같은 방향이므로
// 단순 뺄셈을 쓰면 경계에서 약 2*pi의 가짜 오차가 생긴다.
double wrap_angle(double angle)
{
  return std::atan2(std::sin(angle), std::cos(angle));
}

// SE(2) 상대 pose z_ij = T_i^{-1} T_j를 계산한다.
// 월드 좌표의 위치 차이를 pose i의 로컬 좌표축으로 회전하는 식이다.
Pose2 relative_pose(const Pose2 & from, const Pose2 & to)
{
  const double dx_world = to.x - from.x;
  const double dy_world = to.y - from.y;
  const double c = std::cos(from.yaw);
  const double s = std::sin(from.yaw);
  return Pose2{
    c * dx_world + s * dy_world,
    -s * dx_world + c * dy_world,
    wrap_angle(to.yaw - from.yaw)};
}

// 로컬 상대 이동을 현재 월드 pose에 합성한다: T_next = T_current * T_delta.
Pose2 compose(const Pose2 & current, const Pose2 & delta)
{
  const double c = std::cos(current.yaw);
  const double s = std::sin(current.yaw);
  return Pose2{
    current.x + c * delta.x - s * delta.y,
    current.y + s * delta.x + c * delta.y,
    wrap_angle(current.yaw + delta.yaw)};
}

geometry_msgs::msg::PoseStamped to_pose_stamped(
  const Pose2 & pose, const std_msgs::msg::Header & header)
{
  geometry_msgs::msg::PoseStamped output;
  // Path 안의 모든 PoseStamped는 Path와 동일한 측정시각/좌표계 계약을 지킨다.
  output.header = header;
  output.pose.position.x = pose.x;
  output.pose.position.y = pose.y;
  output.pose.position.z = 0.0;
  // 평면 yaw theta를 단위 quaternion q=[0,0,sin(theta/2),cos(theta/2)]로 바꾼다.
  output.pose.orientation.z = std::sin(0.5 * pose.yaw);
  output.pose.orientation.w = std::cos(0.5 * pose.yaw);
  return output;
}
}  // namespace

class GraphInputSimulator : public rclcpp::Node
{
public:
  GraphInputSimulator()
  : Node("graph_input_simulator")
  {
    // transient_local은 늦게 시작한 optimizer도 가장 최근 graph snapshot을 즉시 받게 한다.
    // depth=1인 이유는 graph가 누적 이벤트가 아니라 완전한 최신 snapshot이기 때문이다.
    const auto snapshot_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    graph_publisher_ = create_publisher<daily_robotics_2026_10_22::msg::PoseGraphInput>(
      "/study/pose_graph_input", snapshot_qos);
    initial_path_publisher_ = create_publisher<nav_msgs::msg::Path>(
      "/study/initial_path", snapshot_qos);

    // 1 Hz wall timer는 합성 front-end snapshot을 반복 제공한다.
    // 실제 SLAM에서는 scan matching/keyframe front-end가 이 역할을 맡는다.
    timer_ = create_wall_timer(
      std::chrono::seconds(1), std::bind(&GraphInputSimulator::publish_snapshot, this));
  }

private:
  void publish_snapshot()
  {
    using Input = daily_robotics_2026_10_22::msg::PoseGraphInput;
    Input input;
    input.header.stamp = now();
    input.header.frame_id = "map";
    input.sequence = ++sequence_;
    input.pose_count = static_cast<std::uint8_t>(kPoseCount);

    // 3 m x 3 m 사각형을 거의 한 바퀴 돈 12개 keyframe의 정답 pose다.
    // 정답은 교육용 독립 auditor만 사용하고 optimizer는 절대 읽지 않는다.
    const std::array<Pose2, kPoseCount> truth{{
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
      {3.0, 0.0, 0.5 * kPi}, {3.0, 1.0, 0.5 * kPi}, {3.0, 2.0, 0.5 * kPi},
      {3.0, 3.0, kPi}, {2.0, 3.0, kPi}, {1.0, 3.0, kPi},
      {0.0, 3.0, -0.5 * kPi}, {0.0, 2.0, -0.5 * kPi}, {0.0, 1.0, -0.5 * kPi}}};

    for (std::size_t i = 0U; i < kPoseCount; ++i) {
      input.truth_x[i] = truth[i].x;
      input.truth_y[i] = truth[i].y;
      input.truth_yaw[i] = truth[i].yaw;
    }

    input.odometry_count = static_cast<std::uint8_t>(kPoseCount - 1U);
    std::array<Pose2, kPoseCount> dead_reckoning{};
    dead_reckoning[0] = truth[0];
    for (std::size_t edge = 0U; edge + 1U < kPoseCount; ++edge) {
      input.odometry_from[edge] = static_cast<std::uint8_t>(edge);
      input.odometry_to[edge] = static_cast<std::uint8_t>(edge + 1U);

      Pose2 measurement = relative_pose(truth[edge], truth[edge + 1U]);
      // 반복 가능한 작은 bias를 넣어 dead reckoning drift를 만든다.
      // 이는 난수가 아니어서 smoke test 결과가 재현 가능하다.
      measurement.x += 0.018;
      measurement.y += (edge % 2U == 0U) ? 0.006 : -0.004;
      measurement.yaw = wrap_angle(measurement.yaw + 0.004);
      input.odometry_dx[edge] = measurement.x;
      input.odometry_dy[edge] = measurement.y;
      input.odometry_dyaw[edge] = measurement.yaw;
      dead_reckoning[edge + 1U] = compose(dead_reckoning[edge], measurement);
    }

    for (std::size_t i = 0U; i < kPoseCount; ++i) {
      input.initial_x[i] = dead_reckoning[i].x;
      input.initial_y[i] = dead_reckoning[i].y;
      input.initial_yaw[i] = dead_reckoning[i].yaw;
    }

    input.loop_count = static_cast<std::uint8_t>(kLoopCount);
    const std::array<std::uint8_t, kLoopCount> loop_from{{0U, 2U, 4U}};
    const std::array<std::uint8_t, kLoopCount> loop_to{{11U, 5U, 10U}};
    for (std::size_t loop = 0U; loop < kLoopCount; ++loop) {
      input.loop_from[loop] = loop_from[loop];
      input.loop_to[loop] = loop_to[loop];
      const Pose2 measurement = relative_pose(truth[loop_from[loop]], truth[loop_to[loop]]);
      input.loop_dx[loop] = measurement.x;
      input.loop_dy[loop] = measurement.y;
      input.loop_dyaw[loop] = measurement.yaw;
      input.loop_expected_inlier[loop] = true;
    }

    // 세 번째 후보는 실제 상대 pose가 아닌 (0,0,0)을 주장하는 false positive다.
    // appearance-based place recognition이 닮은 장소를 잘못 연결한 상황을 흉내 낸다.
    input.loop_dx[2] = 0.0;
    input.loop_dy[2] = 0.0;
    input.loop_dyaw[2] = 0.0;
    input.loop_expected_inlier[2] = false;

    nav_msgs::msg::Path initial_path;
    initial_path.header = input.header;
    initial_path.poses.reserve(kPoseCount);
    for (const auto & pose : dead_reckoning) {
      initial_path.poses.push_back(to_pose_stamped(pose, input.header));
    }

    graph_publisher_->publish(input);
    initial_path_publisher_->publish(initial_path);
    RCLCPP_INFO(
      get_logger(), "graph sequence=%u published: poses=%zu, loops=%zu (1 false)",
      input.sequence, kPoseCount, kLoopCount);
  }

  rclcpp::Publisher<daily_robotics_2026_10_22::msg::PoseGraphInput>::SharedPtr graph_publisher_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr initial_path_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::uint32_t sequence_{0U};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // spin은 executor가 timer callback을 기다렸다가 준비된 callback을 실행하게 한다.
  // 이 노드는 상태 공유가 단순하므로 SingleThreadedExecutor 기본 spin이면 충분하다.
  rclcpp::spin(std::make_shared<GraphInputSimulator>());
  rclcpp::shutdown();
  return 0;
}
