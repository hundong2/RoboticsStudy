#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>

#include "daily_robotics_2026_09_20/msg/terrain_patch.hpp"
#include "daily_robotics_2026_09_20/msg/transport_stats.hpp"
#include "daily_robotics_2026_09_20/msg/traversability_grid.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

// 생산 계산과 별도 노드에서 수식, 시퀀스, 전송 예산을 다시 계산하는 독립 감사기다.
class PipelineAuditor final : public rclcpp::Node {
 public:
  PipelineAuditor() : Node("pipeline_auditor") {
    auto sensor_qos = rclcpp::SensorDataQoS();
    sensor_qos.keep_last(1);
    patch_sub_ = create_subscription<Patch>(
        "/terrain_patch", sensor_qos,
        [this](Patch::ConstSharedPtr message) { store_patch(*message); });

    const auto snapshot_qos = rclcpp::QoS(1).reliable().transient_local();
    grid_sub_ = create_subscription<Grid>(
        "/traversability_fixed", snapshot_qos,
        [this](Grid::ConstSharedPtr message) { store_grid(*message); });
    stats_sub_ = create_subscription<Stats>(
        "/transport_stats", snapshot_qos,
        [this](Stats::ConstSharedPtr message) { store_stats(*message); });
    result_pub_ = create_publisher<std_msgs::msg::String>("/pipeline/audit", snapshot_qos);
  }

 private:
  using Patch = daily_robotics_2026_09_20::msg::TerrainPatch;
  using Stats = daily_robotics_2026_09_20::msg::TransportStats;
  using Grid = daily_robotics_2026_09_20::msg::TraversabilityGrid;
  static constexpr std::size_t kWidth = 8;
  static constexpr std::size_t kSlots = 4;
  static constexpr float kPi = 3.14159265358979323846F;

  static std::size_t cell_index(std::size_t col, std::size_t row) {
    return row * kWidth + col;
  }

  static std::size_t slot(std::uint32_t sequence) {
    return static_cast<std::size_t>(sequence % kSlots);
  }

  // estimator와 공유 라이브러리를 쓰지 않고 같은 수학 정의를 독립 구현한다.
  // 코드 복제는 보통 피하지만, 여기서는 한쪽 버그가 감사 결과까지 그대로 통과하는 것을 막기 위함이다.
  static std::int8_t expected_cost(const Patch & patch, std::size_t col, std::size_t row) {
    if (col == 0U || row == 0U || col + 1U == kWidth || row + 1U == kWidth) { return -1; }
    for (std::size_t y = row - 1U; y <= row + 1U; ++y) {
      for (std::size_t x = col - 1U; x <= col + 1U; ++x) {
        if (patch.valid[cell_index(x, y)] == 0U) { return -1; }
      }
    }
    const double gx =
        (static_cast<double>(patch.elevation_m[cell_index(col + 1U, row)]) -
         static_cast<double>(patch.elevation_m[cell_index(col - 1U, row)])) /
        (2.0 * static_cast<double>(patch.resolution_m));
    const double gy =
        (static_cast<double>(patch.elevation_m[cell_index(col, row + 1U)]) -
         static_cast<double>(patch.elevation_m[cell_index(col, row - 1U)])) /
        (2.0 * static_cast<double>(patch.resolution_m));
    const double slope = std::atan(std::hypot(gx, gy));
    double sum = 0.0;
    for (std::size_t y = row - 1U; y <= row + 1U; ++y) {
      for (std::size_t x = col - 1U; x <= col + 1U; ++x) {
        sum += patch.elevation_m[cell_index(x, y)];
      }
    }
    const double mean = sum / 9.0;
    double squared_error = 0.0;
    for (std::size_t y = row - 1U; y <= row + 1U; ++y) {
      for (std::size_t x = col - 1U; x <= col + 1U; ++x) {
        const double residual = patch.elevation_m[cell_index(x, y)] - mean;
        squared_error += residual * residual;
      }
    }
    const double roughness = std::sqrt(squared_error / 9.0);
    const double slope_limit = 25.0 * static_cast<double>(kPi) / 180.0;
    const double normalized = 0.75 * slope / slope_limit + 0.25 * roughness / 0.04;
    return static_cast<std::int8_t>(
        std::lround(100.0 * std::clamp(normalized, 0.0, 1.0)));
  }

  void store_patch(const Patch & message) {
    const auto i = slot(message.sequence);
    patches_[i] = message;
    patch_ready_[i] = true;
    audit(message.sequence);
  }

  void store_grid(const Grid & message) {
    const auto i = slot(message.sequence);
    grids_[i] = message;
    grid_ready_[i] = true;
    audit(message.sequence);
  }

  void store_stats(const Stats & message) {
    const auto i = slot(message.sequence);
    stats_[i] = message;
    stats_ready_[i] = true;
    audit(message.sequence);
  }

  // 동일 sequence의 세 메시지가 모였을 때만 검사해 비동기 토픽 도착 순서의 영향을 제거한다.
  void audit(std::uint32_t sequence) {
    const auto i = slot(sequence);
    if (sent_ || sequence < 2U || !patch_ready_[i] || !grid_ready_[i] || !stats_ready_[i] ||
        patches_[i].sequence != sequence || grids_[i].sequence != sequence ||
        stats_[i].sequence != sequence) {
      return;
    }

    const auto & patch = patches_[i];
    const auto & grid = grids_[i];
    const auto & stats = stats_[i];
    std::uint8_t expected_valid = 0U;
    std::uint8_t expected_max = 0U;
    bool cells_match = true;
    for (std::size_t row = 0; row < kWidth; ++row) {
      for (std::size_t col = 0; col < kWidth; ++col) {
        const auto expected = expected_cost(patch, col, row);
        cells_match = cells_match && grid.cost[cell_index(col, row)] == expected;
        if (expected >= 0) {
          ++expected_valid;
          expected_max = std::max(expected_max, static_cast<std::uint8_t>(expected));
        }
      }
    }

    const bool transport_match =
        stats.serialized_payload_bytes == 340U && stats.xrce_mtu_bytes == 128U &&
        stats.usable_payload_bytes == 112U && stats.fragment_count == 4U &&
        stats.estimated_wire_bytes == 516U && stats.budget_ok;
    const bool contract_match = patch.width == kWidth && patch.height == kWidth &&
        grid.width == kWidth && grid.height == kWidth && grid.valid_cells == expected_valid &&
        grid.max_cost == expected_max && expected_valid == 36U && expected_max >= 90U;
    const bool pass = cells_match && transport_match && contract_match;

    std_msgs::msg::String result;
    std::ostringstream text;
    text << (pass ? "PASS" : "FAIL") << " seq=" << sequence
         << " valid=" << static_cast<int>(expected_valid)
         << " max_cost=" << static_cast<int>(expected_max)
         << " fragments=" << static_cast<int>(stats.fragment_count)
         << " wire_bytes=" << stats.estimated_wire_bytes;
    result.data = text.str();
    result_pub_->publish(result);
    RCLCPP_INFO(get_logger(), "%s", result.data.c_str());
    sent_ = true;
  }

  rclcpp::Subscription<Patch>::SharedPtr patch_sub_;
  rclcpp::Subscription<Grid>::SharedPtr grid_sub_;
  rclcpp::Subscription<Stats>::SharedPtr stats_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr result_pub_;
  // ROS 생성 메시지는 explicit 기본 생성자를 가지므로 중괄호 집합 초기화 대신 직접 기본 초기화한다.
  std::array<Patch, kSlots> patches_;
  std::array<Grid, kSlots> grids_;
  std::array<Stats, kSlots> stats_;
  std::array<bool, kSlots> patch_ready_{};
  std::array<bool, kSlots> grid_ready_{};
  std::array<bool, kSlots> stats_ready_{};
  bool sent_{false};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PipelineAuditor>());
  rclcpp::shutdown();
  return 0;
}
