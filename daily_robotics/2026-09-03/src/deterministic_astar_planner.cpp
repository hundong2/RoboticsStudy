#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/occupancy_grid.hpp"
#include "nav_msgs/msg/path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/u_int32.hpp"
#include "tf2/time.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"

using namespace std::chrono_literals;

namespace daily_robotics
{

/// 고정 크기 격자에서 A*를 수행한 결과다.
/// std::vector 대신 std::array를 사용해 탐색 도중 동적 메모리 할당이 일어나지 않게 한다.
struct SearchResult
{
  bool found{false};
  std::size_t path_length{0};
  std::uint32_t expanded_nodes{0};
  std::array<int, 400> reverse_path{};
};

/// LiDAR 좌표의 목표점을 map 좌표로 변환하고, 배열 기반 A*로 base_link부터 목표까지 경로를 만든다.
class DeterministicAStarPlanner final : public rclcpp::Node
{
public:
  static constexpr int kWidth = 20;
  static constexpr int kHeight = 20;
  static constexpr int kCellCount = kWidth * kHeight;
  static constexpr double kResolution = 1.0;
  static constexpr double kOriginX = -10.0;
  static constexpr double kOriginY = -10.0;

  DeterministicAStarPlanner()
  : Node("deterministic_astar_planner"),
    // Buffer는 시간에 따른 모든 TF를 캐시하고 target/source frame 사이의 합성 변환을 계산한다.
    tf_buffer_(get_clock()),
    // TransformListener는 /tf와 /tf_static을 구독해 위 Buffer를 채운다.
    // 기본 spin thread를 사용하므로 단일 executor에서도 TF 수신이 목표 콜백과 교착되지 않는다.
    tf_listener_(tf_buffer_)
  {
    build_demo_grid();

    // 목표 Topic의 publisher와 동일한 SensorDataQoS를 사용해야 reliability가 호환된다.
    goal_subscription_ = create_subscription<geometry_msgs::msg::PointStamped>(
      "/planning/goal_in_lidar",
      rclcpp::SensorDataQoS().keep_last(1),
      std::bind(&DeterministicAStarPlanner::on_goal, this, std::placeholders::_1));

    // 경로는 최신 계획 하나만 필요하지만 손실 없이 시각화 노드에 전달하려 reliable depth 1을 쓴다.
    path_publisher_ = create_publisher<nav_msgs::msg::Path>(
      "/planning/path", rclcpp::QoS(1).reliable());

    // transient_local은 늦게 켜진 RViz도 마지막 지도 샘플을 받을 수 있게 하는 DDS durability다.
    grid_publisher_ = create_publisher<nav_msgs::msg::OccupancyGrid>(
      "/planning/grid", rclcpp::QoS(1).reliable().transient_local());

    // UInt32는 고정 크기 메시지라 RMW가 지원하면 loaned message의 장점을 관찰하기 좋다.
    expansion_publisher_ = create_publisher<std_msgs::msg::UInt32>(
      "/planning/expanded_nodes", rclcpp::QoS(1).reliable());

    // 시작 직후 한 번만 지도를 발행한다. transient_local이 이후 구독자를 위해 샘플을 보존한다.
    grid_timer_ = create_wall_timer(500ms, [this]() {
      publish_grid();
      grid_timer_->cancel();
    });

    RCLCPP_INFO(
      get_logger(), "A* planner ready; publisher loan support=%s",
      expansion_publisher_->can_loan_messages() ? "true" : "false");
  }

private:
  enum class NodeState : std::uint8_t
  {
    kUnvisited = 0,
    kOpen = 1,
    kClosed = 2
  };

  /// 가운데 벽에 한 칸의 통로를 둔 20×20 occupancy grid를 만든다.
  void build_demo_grid()
  {
    occupancy_.fill(0);

    // x=10 열을 벽으로 만들되 y=9는 통로로 남겨 A*가 우회 경로를 찾게 한다.
    for (int y = 3; y <= 16; ++y) {
      if (y != 9) {
        occupancy_[to_index(10, y)] = 100;
      }
    }
  }

  /// 2차원 격자 좌표 (x,y)를 연속 메모리의 1차원 인덱스 y*width+x로 바꾼다.
  static constexpr int to_index(const int x, const int y)
  {
    return y * kWidth + x;
  }

  /// 1차원 셀 인덱스에서 x 좌표를 복원한다.
  static constexpr int x_of(const int index)
  {
    return index % kWidth;
  }

  /// 1차원 셀 인덱스에서 y 좌표를 복원한다.
  static constexpr int y_of(const int index)
  {
    return index / kWidth;
  }

  /// map의 연속 좌표를 점유격자 셀로 바꾸고, 지도 밖이면 빈 optional을 반환한다.
  static std::optional<int> world_to_grid(const double world_x, const double world_y)
  {
    // gx=floor((x-origin_x)/resolution)은 월드 좌표가 속한 셀 번호를 뜻한다.
    const int grid_x = static_cast<int>(std::floor((world_x - kOriginX) / kResolution));
    const int grid_y = static_cast<int>(std::floor((world_y - kOriginY) / kResolution));
    if (grid_x < 0 || grid_x >= kWidth || grid_y < 0 || grid_y >= kHeight) {
      return std::nullopt;
    }
    return to_index(grid_x, grid_y);
  }

  /// A*의 admissible/consistent 휴리스틱 h(n)인 4방향 Manhattan 거리를 계산한다.
  static double heuristic(const int from, const int goal)
  {
    return static_cast<double>(
      std::abs(x_of(from) - x_of(goal)) + std::abs(y_of(from) - y_of(goal)));
  }

  /// 고정 크기 배열만 사용해 최단 격자 경로를 찾는다.
  ///
  /// 우선순위 큐 대신 매 반복마다 open 배열을 선형 스캔한다. O(V²)로 느리지만,
  /// 노드 수가 400으로 제한되어 실행 횟수와 메모리 상한이 명확한 교육용 RT 설계다.
  SearchResult run_astar(const int start, const int goal)
  {
    SearchResult result;
    constexpr double kInfinity = std::numeric_limits<double>::infinity();

    g_score_.fill(kInfinity);
    parent_.fill(-1);
    node_state_.fill(NodeState::kUnvisited);

    if (occupancy_[start] != 0 || occupancy_[goal] != 0) {
      return result;
    }

    // g(start)=0이고 f(n)=g(n)+h(n) 중 최소인 open node를 매 단계 확장한다.
    g_score_[start] = 0.0;
    node_state_[start] = NodeState::kOpen;

    constexpr std::array<int, 4> kDx{1, -1, 0, 0};
    constexpr std::array<int, 4> kDy{0, 0, 1, -1};

    for (int iteration = 0; iteration < kCellCount; ++iteration) {
      int current = -1;
      double best_f = kInfinity;
      double best_h = kInfinity;

      for (int index = 0; index < kCellCount; ++index) {
        if (node_state_[index] != NodeState::kOpen) {
          continue;
        }
        const double h = heuristic(index, goal);
        const double f = g_score_[index] + h;

        // f가 같으면 h가 작은, 즉 목표에 가까운 셀을 택해 결과를 재현 가능하게 만든다.
        if (f < best_f || (f == best_f && h < best_h)) {
          current = index;
          best_f = f;
          best_h = h;
        }
      }

      if (current < 0) {
        return result;
      }

      node_state_[current] = NodeState::kClosed;
      ++result.expanded_nodes;

      if (current == goal) {
        // parent를 goal→start 방향으로 따라가므로 reverse_path에 역순으로 저장된다.
        int trace = goal;
        while (trace >= 0 && result.path_length < result.reverse_path.size()) {
          result.reverse_path[result.path_length++] = trace;
          if (trace == start) {
            result.found = true;
            return result;
          }
          trace = parent_[trace];
        }
        return result;
      }

      const int current_x = x_of(current);
      const int current_y = y_of(current);
      for (std::size_t direction = 0; direction < kDx.size(); ++direction) {
        const int next_x = current_x + kDx[direction];
        const int next_y = current_y + kDy[direction];
        if (next_x < 0 || next_x >= kWidth || next_y < 0 || next_y >= kHeight) {
          continue;
        }

        const int next = to_index(next_x, next_y);
        if (occupancy_[next] != 0 || node_state_[next] == NodeState::kClosed) {
          continue;
        }

        // 상하좌우 한 칸 이동 비용 c(current,next)=1이므로 g_new=g_current+1이다.
        const double tentative_g = g_score_[current] + 1.0;
        if (tentative_g < g_score_[next]) {
          g_score_[next] = tentative_g;
          parent_[next] = current;
          node_state_[next] = NodeState::kOpen;
        }
      }
    }

    return result;
  }

  /// LiDAR frame의 목표를 map frame으로 바꾼 뒤 현재 base_link 위치에서 A*를 실행한다.
  void on_goal(const geometry_msgs::msg::PointStamped::SharedPtr goal_in_lidar)
  {
    try {
      // lookupTransform(target, source, time, timeout)은 source 좌표를 target으로 옮기는 변환을 찾는다.
      // TimePointZero는 최신 공통 시각, 100 ms timeout은 TF가 아직 도착하지 않은 시작 구간을 허용한다.
      const auto map_from_lidar = tf_buffer_.lookupTransform(
        "map", goal_in_lidar->header.frame_id, tf2::TimePointZero, 100ms);
      const auto map_from_base = tf_buffer_.lookupTransform(
        "map", "base_link", tf2::TimePointZero, 100ms);

      geometry_msgs::msg::PointStamped goal_in_map;
      // doTransform은 동차변환 p_map = R_map_lidar*p_lidar + t_map_lidar를 적용한다.
      tf2::doTransform(*goal_in_lidar, goal_in_map, map_from_lidar);

      const auto start_cell = world_to_grid(
        map_from_base.transform.translation.x,
        map_from_base.transform.translation.y);
      const auto goal_cell = world_to_grid(goal_in_map.point.x, goal_in_map.point.y);
      if (!start_cell || !goal_cell) {
        RCLCPP_WARN(
          get_logger(), "start or goal is outside the %dx%d map", kWidth, kHeight);
        return;
      }

      const SearchResult result = run_astar(*start_cell, *goal_cell);
      publish_expansion_count(result.expanded_nodes);

      if (!result.found) {
        RCLCPP_WARN(
          get_logger(), "no route: start=%d goal=%d expanded=%u",
          *start_cell, *goal_cell, result.expanded_nodes);
        return;
      }

      publish_path(result);
      RCLCPP_INFO(
        get_logger(), "path cells=%zu, expanded=%u, goal_map=(%.2f, %.2f)",
        result.path_length, result.expanded_nodes, goal_in_map.point.x, goal_in_map.point.y);
    } catch (const tf2::TransformException & exception) {
      // TF tree가 아직 연결되지 않았거나 요청 시각의 변환이 없으면 제어 명령을 만들지 않고 진단한다.
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000, "TF unavailable: %s", exception.what());
    }
  }

  /// 역순 셀 배열을 map frame의 nav_msgs/Path로 바꾸어 시각화/추종 노드에 발행한다.
  void publish_path(const SearchResult & result)
  {
    nav_msgs::msg::Path path;
    path.header.stamp = get_clock()->now();
    path.header.frame_id = "map";

    // Path.poses는 가변 길이 vector이므로 여기서는 비-RT 경계에서 필요한 크기를 한 번 확보한다.
    // 실시간 제어 loop에는 이 메시지를 직접 만들지 말고 고정 크기 setpoint를 넘기는 편이 안전하다.
    path.poses.resize(result.path_length);
    for (std::size_t output_index = 0; output_index < result.path_length; ++output_index) {
      const int cell = result.reverse_path[result.path_length - 1 - output_index];
      auto & pose = path.poses[output_index];
      pose.header = path.header;

      // 셀 중심 좌표는 origin + (cell_index+0.5)*resolution이다.
      pose.pose.position.x = kOriginX + (static_cast<double>(x_of(cell)) + 0.5) * kResolution;
      pose.pose.position.y = kOriginY + (static_cast<double>(y_of(cell)) + 0.5) * kResolution;
      pose.pose.position.z = 0.0;
      pose.pose.orientation.w = 1.0;
    }

    path_publisher_->publish(path);
  }

  /// A*가 확장한 노드 수를 고정 크기 진단 메시지로 발행한다.
  void publish_expansion_count(const std::uint32_t count)
  {
    // can_loan_messages()는 현재 RMW가 publisher용 loan을 실제 지원하는지 런타임에 확인한다.
    if (expansion_publisher_->can_loan_messages()) {
      // borrow_loaned_message는 middleware 소유 메모리를 빌려 new/malloc과 복사를 피할 기회를 준다.
      auto loaned_message = expansion_publisher_->borrow_loaned_message();
      loaned_message.get().data = count;

      // move로 소유권을 publisher에 넘기며 publish 후 LoanedMessage를 다시 사용하면 안 된다.
      expansion_publisher_->publish(std::move(loaned_message));
      return;
    }

    // loan을 지원하지 않는 RMW에서는 동일 동작을 보장하는 일반 메시지 경로로 명시적으로 fallback한다.
    std_msgs::msg::UInt32 message;
    message.data = count;
    expansion_publisher_->publish(message);
  }

  /// RViz에서 장애물과 경로를 함께 볼 수 있도록 정적 occupancy grid를 발행한다.
  void publish_grid()
  {
    nav_msgs::msg::OccupancyGrid grid;
    grid.header.stamp = get_clock()->now();
    grid.header.frame_id = "map";
    grid.info.resolution = static_cast<float>(kResolution);
    grid.info.width = kWidth;
    grid.info.height = kHeight;
    grid.info.origin.position.x = kOriginX;
    grid.info.origin.position.y = kOriginY;
    grid.info.origin.orientation.w = 1.0;

    // ROS OccupancyGrid는 int8 vector를 요구하므로 고정 배열을 한 번 복사한다.
    grid.data.assign(occupancy_.begin(), occupancy_.end());
    grid_publisher_->publish(grid);
  }

  tf2_ros::Buffer tf_buffer_;
  tf2_ros::TransformListener tf_listener_;

  rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr goal_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr path_publisher_;
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr grid_publisher_;
  rclcpp::Publisher<std_msgs::msg::UInt32>::SharedPtr expansion_publisher_;
  rclcpp::TimerBase::SharedPtr grid_timer_;

  std::array<std::int8_t, kCellCount> occupancy_{};
  std::array<double, kCellCount> g_score_{};
  std::array<int, kCellCount> parent_{};
  std::array<NodeState, kCellCount> node_state_{};
};

}  // namespace daily_robotics

int main(int argc, char * argv[])
{
  // rclcpp::init은 DDS/RMW, ROS 인자, logging을 초기화한다.
  rclcpp::init(argc, argv);

  // SingleThreadedExecutor 기반 spin이다. TF listener는 자체 수신 thread를 사용하고 목표 콜백은 순차 실행된다.
  rclcpp::spin(std::make_shared<daily_robotics::DeterministicAStarPlanner>());

  rclcpp::shutdown();
  return 0;
}
