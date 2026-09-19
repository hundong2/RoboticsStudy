#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_09_20/msg/terrain_patch.hpp"
#include "daily_robotics_2026_09_20/msg/traversability_grid.hpp"
#include "nav_msgs/msg/occupancy_grid.hpp"
#include "rclcpp/rclcpp.hpp"

// 높이 패치에서 경사와 국소 거칠기를 계산해 이동 가능 비용 지도를 만든다.
class TraversabilityEstimator final : public rclcpp::Node {
 public:
  TraversabilityEstimator() : Node("traversability_estimator") {
    auto sensor_qos = rclcpp::SensorDataQoS();
    sensor_qos.keep_last(1);
    subscription_ = create_subscription<Patch>(
        "/terrain_patch", sensor_qos,
        [this](Patch::ConstSharedPtr message) { on_patch(*message); });

    // 전체 지도는 상태 스냅샷이므로 늦게 들어온 RViz/테스트도 마지막 값을 받도록 transient_local을 쓴다.
    const auto snapshot_qos = rclcpp::QoS(1).reliable().transient_local();
    fixed_publisher_ = create_publisher<FixedGrid>("/traversability_fixed", snapshot_qos);
    occupancy_publisher_ = create_publisher<nav_msgs::msg::OccupancyGrid>(
        "/traversability_grid", snapshot_qos);
  }

 private:
  using Patch = daily_robotics_2026_09_20::msg::TerrainPatch;
  using FixedGrid = daily_robotics_2026_09_20::msg::TraversabilityGrid;
  static constexpr std::size_t kWidth = 8;
  static constexpr float kPi = 3.14159265358979323846F;
  static constexpr float kSlopeLimitRad = 25.0F * kPi / 180.0F;
  static constexpr float kRoughnessLimitM = 0.04F;

  static std::size_t index(std::size_t col, std::size_t row) {
    return row * kWidth + col;
  }

  // 중앙차분 dz/dx, dz/dy에서 표면 경사 θ=atan(sqrt(gx²+gy²))를 구한다.
  // 3x3 높이 표준편차 σ는 바퀴/발이 느끼는 작은 요철의 대리 지표다.
  static std::int8_t cost_at(const Patch & patch, std::size_t col, std::size_t row) {
    if (col == 0U || row == 0U || col + 1U == kWidth || row + 1U == kWidth) {
      return -1;
    }
    for (std::size_t y = row - 1U; y <= row + 1U; ++y) {
      for (std::size_t x = col - 1U; x <= col + 1U; ++x) {
        if (patch.valid[index(x, y)] == 0U) { return -1; }
      }
    }

    const float dz_dx = (patch.elevation_m[index(col + 1U, row)] -
                         patch.elevation_m[index(col - 1U, row)]) /
                        (2.0F * patch.resolution_m);
    const float dz_dy = (patch.elevation_m[index(col, row + 1U)] -
                         patch.elevation_m[index(col, row - 1U)]) /
                        (2.0F * patch.resolution_m);
    const float slope = std::atan(std::sqrt(dz_dx * dz_dx + dz_dy * dz_dy));

    float mean = 0.0F;
    for (std::size_t y = row - 1U; y <= row + 1U; ++y) {
      for (std::size_t x = col - 1U; x <= col + 1U; ++x) {
        mean += patch.elevation_m[index(x, y)];
      }
    }
    mean /= 9.0F;
    float variance = 0.0F;
    for (std::size_t y = row - 1U; y <= row + 1U; ++y) {
      for (std::size_t x = col - 1U; x <= col + 1U; ++x) {
        const float residual = patch.elevation_m[index(x, y)] - mean;
        variance += residual * residual;
      }
    }
    const float roughness = std::sqrt(variance / 9.0F);

    // C=100·clamp(0.75·θ/θmax + 0.25·σ/σmax, 0, 1).
    const float normalized = 0.75F * (slope / kSlopeLimitRad) +
                             0.25F * (roughness / kRoughnessLimitM);
    return static_cast<std::int8_t>(
        std::lround(100.0F * std::clamp(normalized, 0.0F, 1.0F)));
  }

  // 고정 크기 결과를 먼저 만들고, 같은 데이터를 표준 OccupancyGrid 뷰로 변환한다.
  void on_patch(const Patch & patch) {
    if (patch.width != kWidth || patch.height != kWidth || patch.resolution_m <= 0.0F) {
      RCLCPP_WARN(get_logger(), "invalid patch contract seq=%u", patch.sequence);
      return;
    }

    FixedGrid fixed;
    fixed.stamp = patch.stamp;
    fixed.sequence = patch.sequence;
    fixed.resolution_m = patch.resolution_m;
    fixed.width = patch.width;
    fixed.height = patch.height;
    fixed.valid_cells = 0U;
    fixed.max_cost = 0U;
    for (std::size_t row = 0; row < kWidth; ++row) {
      for (std::size_t col = 0; col < kWidth; ++col) {
        const auto cell_cost = cost_at(patch, col, row);
        fixed.cost[index(col, row)] = cell_cost;
        if (cell_cost >= 0) {
          ++fixed.valid_cells;
          fixed.max_cost = std::max(
              fixed.max_cost, static_cast<std::uint8_t>(cell_cost));
        }
      }
    }
    fixed_publisher_->publish(fixed);

    nav_msgs::msg::OccupancyGrid grid;
    grid.header.stamp = patch.stamp;
    grid.header.frame_id = "terrain_map";
    grid.info.map_load_time = patch.stamp;
    grid.info.resolution = patch.resolution_m;
    grid.info.width = patch.width;
    grid.info.height = patch.height;
    grid.info.origin.position.x = -0.5 * static_cast<double>(patch.width) * patch.resolution_m;
    grid.info.origin.position.y = -0.5 * static_cast<double>(patch.height) * patch.resolution_m;
    grid.info.origin.orientation.w = 1.0;
    // OccupancyGrid의 data는 동적 배열이지만 호스트/RViz 호환 경계에서만 사용한다.
    // row-major 순서는 index=row*width+col이며 -1은 미평가, 0~100은 위험 비용이다.
    grid.data.assign(fixed.cost.begin(), fixed.cost.end());
    occupancy_publisher_->publish(grid);
  }

  rclcpp::Subscription<Patch>::SharedPtr subscription_;
  rclcpp::Publisher<FixedGrid>::SharedPtr fixed_publisher_;
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr occupancy_publisher_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TraversabilityEstimator>());
  rclcpp::shutdown();
  return 0;
}
