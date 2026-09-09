#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav_msgs/msg/occupancy_grid.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{
// 최대 32x32로 입력 계약을 제한한다. 따라서 분석 배열과 BFS queue는 모두 컴파일 시 크기가 정해진다.
constexpr std::size_t kMaxWidth = 32U;
constexpr std::size_t kMaxHeight = 32U;
constexpr std::size_t kMaxCells = kMaxWidth * kMaxHeight;
constexpr std::int8_t kUnknown = -1;
constexpr std::int8_t kFree = 0;

struct AnalysisResult
{
  bool found{false};
  std::size_t frontier_cells{0U};
  std::size_t cluster_count{0U};
  std::size_t best_cluster_size{0U};
  double goal_x{0.0};
  double goal_y{0.0};
  double score{-std::numeric_limits<double>::infinity()};
};
}  // namespace

/**
 * @brief OccupancyGrid에서 free/unknown 경계(frontier)를 찾아 다음 탐사 목표를 발행한다.
 *
 * 핵심 RT 학습 포인트는 "평균이 빠르다"가 아니라 입력 크기와 반복 상한을 계약하는 것이다.
 * N<=1024일 때 frontier 검사 4N, cluster BFS 최대 8N, 대표 cell 선택 N으로 핵심 반복이 13N 이하가 된다.
 */
class BoundedFrontierExplorer : public rclcpp::Node
{
public:
  BoundedFrontierExplorer()
  : Node("bounded_frontier_explorer")
  {
    // 로봇 위치는 frontier까지의 거리 비용을 계산하는 기준점이다.
    robot_x_ = declare_parameter<double>("robot_x", 0.0);
    robot_y_ = declare_parameter<double>("robot_y", 0.0);
    distance_weight_ = declare_parameter<double>("distance_weight", 2.0);
    analysis_budget_us_ = declare_parameter<std::int64_t>("analysis_budget_us", 1500);

    // map publisher와 durability가 맞아야 늦게 시작해도 캐시된 마지막 지도를 받을 수 있다.
    auto map_qos = rclcpp::QoS(rclcpp::KeepLast(1));
    map_qos.reliable().transient_local();
    map_subscription_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
      "map_input", map_qos,
      std::bind(&BoundedFrontierExplorer::on_map, this, std::placeholders::_1));

    // depth 1은 planner가 갱신할 때 소비자에게 가장 최신 goal 하나만 보존한다.
    goal_publisher_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      "goal_output", rclcpp::QoS(1).reliable());
    // String 조립은 동적 메모리를 쓰므로 분석 시간 측정이 끝난 뒤 저주기 진단 경로에서만 수행한다.
    stats_publisher_ = create_publisher<std_msgs::msg::String>(
      "planner_stats", rclcpp::QoS(1).reliable());

    RCLCPP_INFO(
      get_logger(), "frontier explorer: namespace='%s', max_grid=%zux%zu, budget=%ld us",
      get_namespace(), kMaxWidth, kMaxHeight, static_cast<long>(analysis_budget_us_));
  }

private:
  /** (x,y)가 현재 유효한 map 직사각형 안인지 검사한다. */
  bool inside(const int x, const int y) const
  {
    return x >= 0 && y >= 0 &&
           static_cast<std::size_t>(x) < width_ && static_cast<std::size_t>(y) < height_;
  }

  /** 2차원 cell 좌표를 row-major 1차원 배열 index로 바꾼다: i = y*width + x. */
  std::size_t index_of(const int x, const int y) const
  {
    return static_cast<std::size_t>(y) * width_ + static_cast<std::size_t>(x);
  }

  /** free cell의 4-neighbor 중 unknown이 있으면 탐색 영역의 경계인 frontier로 정의한다. */
  bool is_frontier_cell(const int x, const int y) const
  {
    const std::size_t center = index_of(x, y);
    if (grid_[center] != kFree) {
      return false;
    }

    // 4-connectivity는 대각선 하나만 맞닿은 unknown을 frontier로 과대평가하지 않게 한다.
    constexpr std::array<int, 4> kDx{{1, -1, 0, 0}};
    constexpr std::array<int, 4> kDy{{0, 0, 1, -1}};
    for (std::size_t direction = 0U; direction < kDx.size(); ++direction) {
      const int nx = x + kDx[direction];
      const int ny = y + kDy[direction];
      if (inside(nx, ny) && grid_[index_of(nx, ny)] == kUnknown) {
        return true;
      }
    }
    return false;
  }

  /** 고정 배열 위에서 frontier mask 생성, connected component BFS, utility 평가를 수행한다. */
  AnalysisResult analyze()
  {
    AnalysisResult result;
    // fill도 kMaxCells=1024회로 상한이 고정된다. 이전 map의 흔적이 남지 않도록 매번 초기화한다.
    frontier_mask_.fill(0U);
    visited_.fill(0U);

    for (std::size_t y = 0U; y < height_; ++y) {
      for (std::size_t x = 0U; x < width_; ++x) {
        const auto signed_x = static_cast<int>(x);
        const auto signed_y = static_cast<int>(y);
        const std::size_t cell = index_of(signed_x, signed_y);
        if (is_frontier_cell(signed_x, signed_y)) {
          frontier_mask_[cell] = 1U;
          ++result.frontier_cells;
        }
      }
    }

    // 8-connectivity로 서로 붙은 frontier cell을 한 cluster로 묶는다.
    // 각 cell은 visited 된 뒤 queue에 최대 한 번 들어가므로 queue tail은 kMaxCells를 넘지 않는다.
    constexpr std::array<int, 8> kDx{{1, -1, 0, 0, 1, 1, -1, -1}};
    constexpr std::array<int, 8> kDy{{0, 0, 1, -1, 1, -1, 1, -1}};
    const std::size_t active_cells = width_ * height_;

    for (std::size_t seed = 0U; seed < active_cells; ++seed) {
      if (frontier_mask_[seed] == 0U || visited_[seed] != 0U) {
        continue;
      }

      std::size_t head = 0U;
      std::size_t tail = 0U;
      queue_[tail++] = seed;
      visited_[seed] = 1U;
      std::size_t cluster_size = 0U;
      double sum_cell_x = 0.0;
      double sum_cell_y = 0.0;

      while (head < tail) {
        const std::size_t current = queue_[head++];
        const int current_x = static_cast<int>(current % width_);
        const int current_y = static_cast<int>(current / width_);
        ++cluster_size;
        sum_cell_x += static_cast<double>(current_x);
        sum_cell_y += static_cast<double>(current_y);

        for (std::size_t direction = 0U; direction < kDx.size(); ++direction) {
          const int nx = current_x + kDx[direction];
          const int ny = current_y + kDy[direction];
          if (!inside(nx, ny)) {
            continue;
          }
          const std::size_t neighbor = index_of(nx, ny);
          if (frontier_mask_[neighbor] != 0U && visited_[neighbor] == 0U) {
            visited_[neighbor] = 1U;
            queue_[tail++] = neighbor;
          }
        }
      }

      ++result.cluster_count;
      const double centroid_cell_x = sum_cell_x / static_cast<double>(cluster_size);
      const double centroid_cell_y = sum_cell_y / static_cast<double>(cluster_size);
      // cell 중심을 world로 변환한다: p_world = origin + resolution*(cell + 1/2).
      const double centroid_x = origin_x_ + resolution_ * (centroid_cell_x + 0.5);
      const double centroid_y = origin_y_ + resolution_ * (centroid_cell_y + 0.5);
      const double distance_m = std::hypot(centroid_x - robot_x_, centroid_y - robot_y_);

      // 산술 centroid는 U자 모양 cluster의 장애물/unknown 안에 놓일 수 있다.
      // queue에 남아 있는 실제 frontier cell 중 centroid와 가장 가까운 cell을 대표 목표로 골라
      // 최소한 occupancy 의미상 free인 cell만 발행한다. 이 추가 순회도 cluster 전체에서 총 N회 이하이다.
      std::size_t representative = queue_[0U];
      double representative_distance_sq = std::numeric_limits<double>::infinity();
      for (std::size_t member = 0U; member < tail; ++member) {
        const double member_x = static_cast<double>(queue_[member] % width_);
        const double member_y = static_cast<double>(queue_[member] / width_);
        const double dx = member_x - centroid_cell_x;
        const double dy = member_y - centroid_cell_y;
        const double squared_distance = dx * dx + dy * dy;
        if (squared_distance < representative_distance_sq) {
          representative_distance_sq = squared_distance;
          representative = queue_[member];
        }
      }
      const double representative_cell_x = static_cast<double>(representative % width_);
      const double representative_cell_y = static_cast<double>(representative / width_);
      const double representative_x = origin_x_ + resolution_ * (representative_cell_x + 0.5);
      const double representative_y = origin_y_ + resolution_ * (representative_cell_y + 0.5);

      // U(C)=|C|-lambda*d: 넓은 미지 영역 경계를 선호하되 이동 거리가 긴 목표에는 비용을 준다.
      const double score = static_cast<double>(cluster_size) - distance_weight_ * distance_m;
      if (score > result.score) {
        result.found = true;
        result.best_cluster_size = cluster_size;
        result.goal_x = representative_x;
        result.goal_y = representative_y;
        result.score = score;
      }
    }

    return result;
  }

  /** 새 map을 고정 배열에 복사하고 bounded frontier 분석 결과를 goal과 통계로 발행한다. */
  void on_map(const nav_msgs::msg::OccupancyGrid::SharedPtr msg)
  {
    const std::size_t width = static_cast<std::size_t>(msg->info.width);
    const std::size_t height = static_cast<std::size_t>(msg->info.height);
    // 크기 계약 위반을 즉시 거부해야 fixed buffer overflow가 불가능하다.
    if (width == 0U || height == 0U || width > kMaxWidth || height > kMaxHeight)
    {
      RCLCPP_ERROR(
        get_logger(), "map rejected: got %zux%zu, contract is 1x1..%zux%zu",
        width, height, kMaxWidth, kMaxHeight);
      return;
    }
    const std::size_t expected_cells = width * height;
    if (msg->data.size() != expected_cells) {
      RCLCPP_ERROR(
        get_logger(), "map rejected: metadata requires %zu cells but data has %zu",
        expected_cells, msg->data.size());
      return;
    }

    width_ = width;
    height_ = height;
    resolution_ = static_cast<double>(msg->info.resolution);
    origin_x_ = msg->info.origin.position.x;
    origin_y_ = msg->info.origin.position.y;
    map_frame_ = msg->header.frame_id.empty() ? "map" : msg->header.frame_id;
    // std::copy_n은 정확히 expected_cells만 복사한다. 목적 배열은 최대 1024칸으로 이미 할당되어 있다.
    std::copy_n(msg->data.begin(), expected_cells, grid_.begin());

    // steady_clock은 ROS /clock 점프와 무관한 단조 시계라 callback 계산시간 계측에 적합하다.
    const auto started = std::chrono::steady_clock::now();
    const AnalysisResult result = analyze();
    const auto finished = std::chrono::steady_clock::now();
    const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
      finished - started).count();
    worst_analysis_us_ = std::max(worst_analysis_us_, elapsed_us);

    if (elapsed_us > analysis_budget_us_) {
      ++budget_misses_;
      RCLCPP_WARN(
        get_logger(), "analysis budget miss: elapsed=%ld us budget=%ld us",
        static_cast<long>(elapsed_us), static_cast<long>(analysis_budget_us_));
    }

    if (result.found) {
      geometry_msgs::msg::PoseStamped goal;
      goal.header.stamp = now();
      goal.header.frame_id = map_frame_;
      goal.pose.position.x = result.goal_x;
      goal.pose.position.y = result.goal_y;
      // 위치 목표만 쓰므로 회전은 단위 quaternion으로 둔다. 실제 Nav2에서는 진행 방향 yaw를 정한다.
      goal.pose.orientation.w = 1.0;
      goal_publisher_->publish(goal);
    }

    // 사람이 보는 진단은 계산 측정 구간 밖에서 문자열로 만든다. 따라서 RT hot path 주장에 포함하지 않는다.
    std_msgs::msg::String stats;
    std::ostringstream stream;
    stream << "namespace=" << get_namespace()
           << " cells=" << expected_cells
           << " frontier_cells=" << result.frontier_cells
           << " clusters=" << result.cluster_count
           << " best_cluster=" << result.best_cluster_size
           << " elapsed_us=" << elapsed_us
           << " worst_us=" << worst_analysis_us_
           << " budget_misses=" << budget_misses_;
    stats.data = stream.str();
    stats_publisher_->publish(stats);

    ++map_count_;
    if (map_count_ % 4U == 1U) {
      RCLCPP_INFO(
        get_logger(), "frontier=%zu clusters=%zu goal=(%.2f, %.2f) analysis=%ld us",
        result.frontier_cells, result.cluster_count, result.goal_x, result.goal_y,
        static_cast<long>(elapsed_us));
    }
  }

  double robot_x_{0.0};
  double robot_y_{0.0};
  double distance_weight_{2.0};
  std::int64_t analysis_budget_us_{1500};
  std::size_t width_{0U};
  std::size_t height_{0U};
  double resolution_{0.0};
  double origin_x_{0.0};
  double origin_y_{0.0};
  std::string map_frame_{"map"};
  std::uint64_t map_count_{0U};
  std::int64_t worst_analysis_us_{0};
  std::uint64_t budget_misses_{0U};

  std::array<std::int8_t, kMaxCells> grid_{};
  std::array<std::uint8_t, kMaxCells> frontier_mask_{};
  std::array<std::uint8_t, kMaxCells> visited_{};
  std::array<std::size_t, kMaxCells> queue_{};

  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr map_subscription_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr goal_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr stats_publisher_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init이 ROS 인수, DDS/RMW context와 signal handler를 준비한다.
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor의 spin은 map callback을 직렬 실행해 공유 배열에 동시 접근이 생기지 않게 한다.
  rclcpp::spin(std::make_shared<BoundedFrontierExplorer>());
  rclcpp::shutdown();
  return 0;
}
