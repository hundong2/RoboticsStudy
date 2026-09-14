#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "daily_robotics_2026_09_15/msg/joint_space_world.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "trajectory_msgs/msg/joint_trajectory.hpp"
#include "trajectory_msgs/msg/joint_trajectory_point.hpp"

namespace daily_robotics
{

namespace
{

constexpr std::size_t kMaxObstacles = 8;
constexpr std::size_t kMaxNodes = 512;
constexpr std::size_t kMaxPathPoints = kMaxNodes * 2;
constexpr std::size_t kMaxIterations = 600;
constexpr double kPi = 3.14159265358979323846;
constexpr double kStep = 0.14;
constexpr double kCollisionResolution = 0.025;
constexpr double kSafetyMargin = 0.16;

struct Config
{
  double q1{0.0};
  double q2{0.0};
};

struct Obstacle
{
  double q1{0.0};
  double q2{0.0};
  double radius{0.0};
};

struct World
{
  Config start;
  Config goal;
  std::array<Obstacle, kMaxObstacles> obstacles{};
  std::size_t obstacle_count{0};
};

struct TreeNode
{
  Config q;
  int parent{-1};
};

// std::array 기반 Tree는 계획 callback 도중 노드 vector가 재할당되는 일을 막는다.
// size가 kMaxNodes에 도달하면 더 키우지 않고 실패를 보고하므로 메모리 상한이 명확하다.
struct Tree
{
  std::array<TreeNode, kMaxNodes> nodes{};
  std::size_t size{0};
};

enum class ExtendResult
{
  trapped,
  advanced,
  reached
};

double distance(const Config & a, const Config & b)
{
  // d(q_a,q_b)=sqrt((q1a-q1b)^2+(q2a-q2b)^2): 2축 관절 공간의 유클리드 거리다.
  return std::hypot(a.q1 - b.q1, a.q2 - b.q2);
}

}  // namespace

// 이 노드는 world snapshot을 받아 시작과 목표에서 두 RRT를 키운 뒤 JointTrajectory를 발행한다.
// 학습 포인트는 랜덤 planner 자체보다 노드/반복/충돌 샘플 수의 상한을 코드로 드러내는 데 있다.
class BoundedRrtConnectNode final : public rclcpp::Node
{
public:
  BoundedRrtConnectNode()
  : Node("bounded_rrt_connect")
  {
    const auto latched_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();

    // JointTrajectory는 joint_names 순서와 각 point.positions 순서가 일치해야 하는 표준 계약이다.
    trajectory_pub_ = create_publisher<trajectory_msgs::msg::JointTrajectory>(
      "/planning/joint_trajectory", latched_qos);
    diagnostics_pub_ = create_publisher<std_msgs::msg::String>(
      "/planning/rrt_diagnostics", latched_qos);

    // TransientLocal 구독은 world publisher가 먼저 보낸 마지막 snapshot도 DDS history에서 받는다.
    world_sub_ = create_subscription<daily_robotics_2026_09_15::msg::JointSpaceWorld>(
      "/planning/joint_space_world", latched_qos,
      std::bind(&BoundedRrtConnectNode::on_world, this, std::placeholders::_1));
  }

private:
  // 입력 메시지를 고정 용량 계산 구조로 복사하고, 유효한 world일 때 한 번의 planning query를 수행한다.
  void on_world(const daily_robotics_2026_09_15::msg::JointSpaceWorld::SharedPtr msg)
  {
    World world;
    world.start = {msg->start[0], msg->start[1]};
    world.goal = {msg->goal[0], msg->goal[1]};
    world.obstacle_count = std::min(msg->obstacles.size(), kMaxObstacles);
    for (std::size_t i = 0; i < world.obstacle_count; ++i) {
      world.obstacles[i] = {
        msg->obstacles[i].x, msg->obstacles[i].y, std::max(0.0, msg->obstacles[i].z)};
    }

    if (!collision_free(world.start, world) || !collision_free(world.goal, world)) {
      publish_failure("start_or_goal_in_collision");
      return;
    }

    const auto begin = std::chrono::steady_clock::now();
    Tree start_tree;
    Tree goal_tree;
    start_tree.nodes[0] = {world.start, -1};
    goal_tree.nodes[0] = {world.goal, -1};
    start_tree.size = 1;
    goal_tree.size = 1;

    std::size_t start_connection = 0;
    std::size_t goal_connection = 0;
    std::size_t used_iterations = 0;
    bool solved = false;
    bool grow_start = true;

    for (std::size_t iteration = 0; iteration < kMaxIterations; ++iteration) {
      used_iterations = iteration + 1;
      Tree & active = grow_start ? start_tree : goal_tree;
      Tree & passive = grow_start ? goal_tree : start_tree;

      // 10회 중 1회는 반대쪽 root를 직접 겨냥하고, 나머지는 C-space를 균일 표본화한다.
      // xorshift32의 seed를 고정해 교육/검증 결과를 재현 가능하게 만든다.
      const Config sample = (iteration % 10 == 0) ?
        (grow_start ? world.goal : world.start) : random_config();

      std::size_t active_new = 0;
      const auto active_result = extend(active, sample, world, active_new);
      if (active_result != ExtendResult::trapped) {
        std::size_t passive_new = 0;
        const auto passive_result = connect(passive, active.nodes[active_new].q, world, passive_new);
        if (passive_result == ExtendResult::reached) {
          if (grow_start) {
            start_connection = active_new;
            goal_connection = passive_new;
          } else {
            start_connection = passive_new;
            goal_connection = active_new;
          }
          solved = true;
          break;
        }
      }

      // 두 트리를 번갈아 키우면 start/goal 어느 한쪽의 좁은 영역에 탐색이 편향되는 것을 줄인다.
      grow_start = !grow_start;
    }

    if (!solved) {
      publish_failure("bounded_search_exhausted");
      return;
    }

    std::array<Config, kMaxPathPoints> path{};
    const std::size_t path_count = reconstruct_path(
      start_tree, start_connection, goal_tree, goal_connection, path);
    const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - begin).count();
    publish_solution(
      path, path_count, world, used_iterations, start_tree.size, goal_tree.size, elapsed_us);
  }

  // xorshift32는 암호용이 아니라 heap 없이 재현 가능한 학습용 균일 표본을 만드는 PRNG다.
  std::uint32_t next_random()
  {
    rng_state_ ^= rng_state_ << 13;
    rng_state_ ^= rng_state_ >> 17;
    rng_state_ ^= rng_state_ << 5;
    return rng_state_;
  }

  Config random_config()
  {
    constexpr double denominator = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    const double u1 = static_cast<double>(next_random()) / denominator;
    const double u2 = static_cast<double>(next_random()) / denominator;
    return {-kPi + 2.0 * kPi * u1, -kPi + 2.0 * kPi * u2};
  }

  bool collision_free(const Config & q, const World & world) const
  {
    if (q.q1 < -kPi || q.q1 > kPi || q.q2 < -kPi || q.q2 > kPi) {
      return false;
    }
    for (std::size_t i = 0; i < world.obstacle_count; ++i) {
      const auto & obstacle = world.obstacles[i];
      // ||q-c|| > r+margin 이면 원형 C-space obstacle 밖이다.
      // margin은 모델 오차와 servo 제동 거리를 흉내 내는 보수적 팽창이다.
      if (std::hypot(q.q1 - obstacle.q1, q.q2 - obstacle.q2) <=
        obstacle.radius + kSafetyMargin)
      {
        return false;
      }
    }
    return true;
  }

  bool segment_free(const Config & from, const Config & to, const World & world) const
  {
    const double length = distance(from, to);
    const std::size_t requested = static_cast<std::size_t>(
      std::ceil(length / kCollisionResolution));
    // 한 RRT edge 길이는 kStep 이하이므로 보통 6회다. 방어적으로 64회 상한을 둔다.
    const std::size_t steps = std::clamp<std::size_t>(requested, 1, 64);
    for (std::size_t i = 1; i <= steps; ++i) {
      const double alpha = static_cast<double>(i) / static_cast<double>(steps);
      // q(alpha)=(1-alpha)q_from+alpha q_to: 관절 공간 직선 local planner다.
      const Config interpolated{
        from.q1 + alpha * (to.q1 - from.q1),
        from.q2 + alpha * (to.q2 - from.q2)};
      if (!collision_free(interpolated, world)) {
        return false;
      }
    }
    return true;
  }

  std::size_t nearest(const Tree & tree, const Config & target) const
  {
    std::size_t best_index = 0;
    double best_distance = std::numeric_limits<double>::max();
    // 선형 최근접 탐색은 O(N)이지만 N<=512라 실행량이 명확하다. 대규모 문제에서는 k-d tree가 낫다.
    for (std::size_t i = 0; i < tree.size; ++i) {
      const double candidate = distance(tree.nodes[i].q, target);
      if (candidate < best_distance) {
        best_distance = candidate;
        best_index = i;
      }
    }
    return best_index;
  }

  ExtendResult extend(
    Tree & tree, const Config & target, const World & world, std::size_t & new_index) const
  {
    if (tree.size >= kMaxNodes) {
      return ExtendResult::trapped;
    }
    const std::size_t near_index = nearest(tree, target);
    const Config near = tree.nodes[near_index].q;
    const double d = distance(near, target);
    if (d < 1e-12) {
      new_index = near_index;
      return ExtendResult::reached;
    }

    const double travel = std::min(kStep, d);
    // q_new=q_near+(travel/d)(q_target-q_near): 방향은 유지하고 한 번에 kStep만 전진한다.
    const Config proposed{
      near.q1 + (travel / d) * (target.q1 - near.q1),
      near.q2 + (travel / d) * (target.q2 - near.q2)};
    if (!segment_free(near, proposed, world)) {
      return ExtendResult::trapped;
    }

    new_index = tree.size;
    tree.nodes[tree.size++] = {proposed, static_cast<int>(near_index)};
    return d <= kStep ? ExtendResult::reached : ExtendResult::advanced;
  }

  ExtendResult connect(
    Tree & tree, const Config & target, const World & world, std::size_t & new_index) const
  {
    // CONNECT는 trapped/reached까지 같은 target으로 greedy EXTEND를 반복한다.
    // 각 반복마다 노드가 하나 늘고 Tree 용량이 512이므로 이 while도 유한하다.
    for (std::size_t attempt = 0; attempt < kMaxNodes; ++attempt) {
      const auto result = extend(tree, target, world, new_index);
      if (result != ExtendResult::advanced) {
        return result;
      }
    }
    return ExtendResult::trapped;
  }

  std::size_t reconstruct_path(
    const Tree & start_tree, std::size_t start_connection,
    const Tree & goal_tree, std::size_t goal_connection,
    std::array<Config, kMaxPathPoints> & output) const
  {
    std::array<Config, kMaxNodes> reverse_start{};
    std::size_t reverse_count = 0;
    int index = static_cast<int>(start_connection);
    while (index >= 0 && reverse_count < reverse_start.size()) {
      reverse_start[reverse_count++] = start_tree.nodes[static_cast<std::size_t>(index)].q;
      index = start_tree.nodes[static_cast<std::size_t>(index)].parent;
    }

    std::size_t count = 0;
    for (std::size_t i = reverse_count; i > 0; --i) {
      output[count++] = reverse_start[i - 1];
    }

    // goal tree의 parent 방향은 connection→goal이므로 그대로 따라가면 start→goal 경로가 완성된다.
    index = static_cast<int>(goal_connection);
    bool first_goal_node = true;
    while (index >= 0 && count < output.size()) {
      const Config q = goal_tree.nodes[static_cast<std::size_t>(index)].q;
      if (!(first_goal_node && count > 0 && distance(output[count - 1], q) < 1e-9)) {
        output[count++] = q;
      }
      first_goal_node = false;
      index = goal_tree.nodes[static_cast<std::size_t>(index)].parent;
    }
    return count;
  }

  void publish_solution(
    const std::array<Config, kMaxPathPoints> & path, std::size_t count, const World & world,
    std::size_t iterations, std::size_t start_nodes, std::size_t goal_nodes,
    long long elapsed_us)
  {
    trajectory_msgs::msg::JointTrajectory trajectory;
    trajectory.header.stamp = now();
    trajectory.header.frame_id = "joint_space";
    trajectory.joint_names = {"joint1", "joint2"};
    trajectory.points.reserve(count);

    double elapsed_seconds = 0.0;
    double path_length = 0.0;
    double min_clearance = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < count; ++i) {
      trajectory_msgs::msg::JointTrajectoryPoint point;
      point.positions = {path[i].q1, path[i].q2};
      if (i > 0) {
        const double segment_length = distance(path[i - 1], path[i]);
        path_length += segment_length;
        // time_from_start는 궤적 시작 기준 누적 시간이다. 여기서는 nominal 1.2 rad/s로 시간화한다.
        elapsed_seconds += segment_length / 1.2;
      }
      // builtin_interfaces/Duration을 직접 채우면 rclcpp::Duration 변환 API의 배포판 차이를 피할 수 있다.
      const auto elapsed_nanoseconds = static_cast<std::int64_t>(
        std::llround(elapsed_seconds * 1'000'000'000.0));
      point.time_from_start.sec = static_cast<std::int32_t>(elapsed_nanoseconds / 1'000'000'000LL);
      point.time_from_start.nanosec = static_cast<std::uint32_t>(
        elapsed_nanoseconds % 1'000'000'000LL);
      trajectory.points.push_back(point);

      for (std::size_t obstacle_index = 0; obstacle_index < world.obstacle_count;
        ++obstacle_index)
      {
        const auto & obstacle = world.obstacles[obstacle_index];
        min_clearance = std::min(
          min_clearance,
          std::hypot(path[i].q1 - obstacle.q1, path[i].q2 - obstacle.q2) - obstacle.radius);
      }
    }
    trajectory_pub_->publish(trajectory);

    std_msgs::msg::String diagnostics;
    std::ostringstream stream;
    stream << "status=SOLVED iterations=" << iterations
           << " start_nodes=" << start_nodes
           << " goal_nodes=" << goal_nodes
           << " points=" << count
           << " path_length_rad=" << path_length
           << " min_waypoint_clearance_rad=" << min_clearance
           << " planning_us=" << elapsed_us;
    diagnostics.data = stream.str();
    diagnostics_pub_->publish(diagnostics);
    RCLCPP_INFO(get_logger(), "%s", diagnostics.data.c_str());
  }

  void publish_failure(const std::string & reason)
  {
    std_msgs::msg::String diagnostics;
    diagnostics.data = "status=FAILED reason=" + reason;
    diagnostics_pub_->publish(diagnostics);
    RCLCPP_ERROR(get_logger(), "%s", diagnostics.data.c_str());
  }

  std::uint32_t rng_state_{0x6D2B79F5u};
  rclcpp::Publisher<trajectory_msgs::msg::JointTrajectory>::SharedPtr trajectory_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diagnostics_pub_;
  rclcpp::Subscription<daily_robotics_2026_09_15::msg::JointSpaceWorld>::SharedPtr world_sub_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<daily_robotics::BoundedRrtConnectNode>());
  rclcpp::shutdown();
  return 0;
}
