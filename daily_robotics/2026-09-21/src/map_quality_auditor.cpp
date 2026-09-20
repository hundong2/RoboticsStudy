#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>

#include "nav_msgs/msg/occupancy_grid.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"
#include "std_msgs/msg/string.hpp"

namespace {
constexpr double kWallX = 4.0;

struct Quality {
  double mean_abs_x_error{std::numeric_limits<double>::infinity()};
  double near_wall_ratio{0.0};
  std::uint32_t occupied_cells{0};
};
}  // namespace

// 생산 mapper와 다른 계산으로 raw/deskew 지도의 벽 집중도를 비교한다.
// 로봇 시스템에서의 역할은 회귀 테스트가 "지도는 나왔지만 deskew가 무효"인 오류를 잡는 것이다.
class MapQualityAuditor final : public rclcpp::Node {
 public:
  MapQualityAuditor() : Node("map_quality_auditor") {
    const auto snapshot_qos = rclcpp::QoS(1).reliable().transient_local();
    raw_sub_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
        "/mapping/raw", snapshot_qos,
        [this](nav_msgs::msg::OccupancyGrid::ConstSharedPtr map) {
          raw_map_ = *map;
          raw_seen_ = true;
          evaluate();
        });
    deskew_sub_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
        "/mapping/deskewed", snapshot_qos,
        [this](nav_msgs::msg::OccupancyGrid::ConstSharedPtr map) {
          deskew_map_ = *map;
          deskew_seen_ = true;
          evaluate();
        });
    stats_sub_ = create_subscription<std_msgs::msg::Float64MultiArray>(
        "/mapping/stats", snapshot_qos,
        [this](std_msgs::msg::Float64MultiArray::ConstSharedPtr stats) {
          if (stats->data.size() == 9) {
            stats_ = *stats;
            stats_seen_ = true;
            evaluate();
          }
        });
    result_pub_ = create_publisher<std_msgs::msg::String>("/mapping/audit", snapshot_qos);
  }

 private:
  // OccupancyGrid 계약과 x=4 m 벽 주변의 점유 분포를 독립 계산한다.
  Quality measure(const nav_msgs::msg::OccupancyGrid & map) const {
    Quality quality;
    double weighted_error = 0.0;
    double total_weight = 0.0;
    double near_weight = 0.0;
    for (std::uint32_t row = 0; row < map.info.height; ++row) {
      for (std::uint32_t col = 0; col < map.info.width; ++col) {
        const auto index = static_cast<std::size_t>(row) * map.info.width + col;
        const int probability = static_cast<int>(map.data[index]);
        if (probability < 58) {
          continue;  // free/unknown/약한 셀은 벽 두께 지표에서 제외한다.
        }
        const double x = map.info.origin.position.x +
          (static_cast<double>(col) + 0.5) * map.info.resolution;
        const double error = std::abs(x - kWallX);
        const double weight = static_cast<double>(probability - 50);
        weighted_error += weight * error;
        total_weight += weight;
        if (error <= 0.11) {
          near_weight += weight;
        }
        ++quality.occupied_cells;
      }
    }
    if (total_weight > 0.0) {
      quality.mean_abs_x_error = weighted_error / total_weight;
      quality.near_wall_ratio = near_weight / total_weight;
    }
    return quality;
  }

  bool valid_map_contract(const nav_msgs::msg::OccupancyGrid & map) const {
    return map.header.frame_id == "map" && map.info.width == 100 && map.info.height == 100 &&
      std::abs(map.info.resolution - 0.10F) < 1.0e-6F && map.data.size() == 10000 &&
      std::abs(map.info.origin.position.x) < 1.0e-9 &&
      std::abs(map.info.origin.position.y + 5.0) < 1.0e-9 &&
      std::abs(map.info.origin.orientation.w - 1.0) < 1.0e-9;
  }

  // 최소 30개 스캔 뒤 raw 대비 deskew 개선, 상한, TF 실패를 함께 판정한다.
  // 너무 이른 지도는 관측 각도가 좁아 raw/deskew 차이를 안정적으로 분리하기 어렵다.
  void evaluate() {
    if (sent_ || !raw_seen_ || !deskew_seen_ || !stats_seen_) {
      return;
    }
    const std::uint32_t received = static_cast<std::uint32_t>(stats_.data[0]);
    const std::uint32_t rejected = static_cast<std::uint32_t>(stats_.data[1]);
    const std::uint32_t processed = static_cast<std::uint32_t>(stats_.data[2]);
    const std::uint32_t tf_failures = static_cast<std::uint32_t>(stats_.data[3]);
    const std::uint32_t max_beams = static_cast<std::uint32_t>(stats_.data[5]);
    const std::uint32_t max_steps = static_cast<std::uint32_t>(stats_.data[6]);
    const double max_callback_us = stats_.data[7];
    const double max_sigma_m = stats_.data[8];
    if (processed < 30) {
      return;
    }

    const Quality raw = measure(raw_map_);
    const Quality deskew = measure(deskew_map_);
    const bool pass = valid_map_contract(raw_map_) && valid_map_contract(deskew_map_) &&
      received >= processed && rejected == 0 && tf_failures <= 2 &&
      max_beams <= 181 && max_steps <= 100 && max_steps > 0 &&
      raw.occupied_cells >= 8 && deskew.occupied_cells >= 8 &&
      deskew.mean_abs_x_error < 0.09 &&
      deskew.mean_abs_x_error < raw.mean_abs_x_error * 0.80 &&
      deskew.near_wall_ratio > raw.near_wall_ratio + 0.10 &&
      max_sigma_m > 0.0 && max_sigma_m < 0.10;

    std::ostringstream stream;
    stream << (pass ? "PASS" : "FAIL")
           << " processed=" << processed
           << " tf_failures=" << tf_failures
           << " raw_error_m=" << raw.mean_abs_x_error
           << " deskew_error_m=" << deskew.mean_abs_x_error
           << " raw_near=" << raw.near_wall_ratio
           << " deskew_near=" << deskew.near_wall_ratio
           << " max_beams=" << max_beams
           << " max_steps=" << max_steps
           << " max_callback_us=" << max_callback_us
           << " max_sigma_m=" << max_sigma_m;
    std_msgs::msg::String result;
    result.data = stream.str();
    // transient_local 결과는 smoke test가 뒤늦게 접속해도 마지막 PASS/FAIL을 받는다.
    result_pub_->publish(result);
    RCLCPP_INFO(get_logger(), "%s", result.data.c_str());
    sent_ = true;
  }

  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr raw_sub_;
  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr deskew_sub_;
  rclcpp::Subscription<std_msgs::msg::Float64MultiArray>::SharedPtr stats_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr result_pub_;
  nav_msgs::msg::OccupancyGrid raw_map_;
  nav_msgs::msg::OccupancyGrid deskew_map_;
  std_msgs::msg::Float64MultiArray stats_;
  bool raw_seen_{false};
  bool deskew_seen_{false};
  bool stats_seen_{false};
  bool sent_{false};
};

int main(int argc, char ** argv) {
  // spin은 세 스냅샷 Topic을 수신하고 조건이 만족될 때 독립 판정을 발행한다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MapQualityAuditor>());
  rclcpp::shutdown();
  return 0;
}
