#include <cstdint>
#include <memory>
#include <sstream>

#include "nav_msgs/msg/occupancy_grid.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/u_int32_multi_array.hpp"

// 지도 제작 코드와 별도 노드에서 알려진 벽·자유공간·미관측 셀을 검증한다.
class MapAuditor final : public rclcpp::Node {
 public:
  MapAuditor() : Node("map_auditor") {
    // reliable/transient_local은 mapper가 마지막으로 낸 전체 지도와 통계를 늦은 구독자에게 준다.
    const auto snapshot_qos = rclcpp::QoS(1).reliable().transient_local();
    map_sub_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
        "/map", snapshot_qos,
        [this](nav_msgs::msg::OccupancyGrid::ConstSharedPtr msg) { on_map(*msg); });
    stats_sub_ = create_subscription<std_msgs::msg::UInt32MultiArray>(
        "/map/stats", snapshot_qos,
        [this](std_msgs::msg::UInt32MultiArray::ConstSharedPtr msg) { on_stats(*msg); });
    result_pub_ = create_publisher<std_msgs::msg::String>("/map/audit", snapshot_qos);
  }

 private:
  // x,y(미터)를 지도 배열의 row-major 인덱스로 독립 계산해 mapper의 셀 배치를 확인한다.
  std::int8_t at(const nav_msgs::msg::OccupancyGrid & map, int col, int row) const {
    return map.data[static_cast<std::size_t>(row) * map.info.width + col];
  }

  // 알려진 x=2 m 벽, x=1 m 자유공간, x=3 m 벽 뒤 unknown을 스냅샷에서 검사한다.
  void on_map(const nav_msgs::msg::OccupancyGrid & map) {
    if (map.header.frame_id != "map" || map.info.width != 80 || map.info.height != 80 ||
        map.info.resolution != 0.1F || map.data.size() != 6400 ||
        map.info.origin.position.x != -4.0 || map.info.origin.position.y != -4.0 ||
        map.info.origin.orientation.w != 1.0) {
      map_contract_ok_ = false;
      map_seen_ = true;
      publish_if_ready();
      return;
    }
    map_contract_ok_ = true;
    // 셀 중심 기준 x=1→col 50, x=2→col 60, x=3→col 70, y=0→row 40.
    free_probability_ = at(map, 50, 40);
    wall_probability_ = at(map, 60, 40);
    behind_wall_ = at(map, 70, 40);
    map_seen_ = true;
    publish_if_ready();
  }

  // mapper가 보고한 고정 처리 한계와 실제 처리 스캔 수를 받아 검증한다.
  void on_stats(const std_msgs::msg::UInt32MultiArray & stats) {
    if (stats.data.size() != 5) { return; }
    received_ = stats.data[0];
    rejected_ = stats.data[1];
    processed_ = stats.data[2];
    replaced_ = stats.data[3];
    max_steps_ = stats.data[4];
    publish_if_ready();
  }

  // 세 번 이상 적분한 뒤 PASS/FAIL을 발행한다. 테스트가 정확한 계약과 처리량을 확인한다.
  void publish_if_ready() {
    if (!map_seen_ || processed_ < 3 || sent_) { return; }
    const bool pass = map_contract_ok_ && received_ >= processed_ && rejected_ == 0 &&
                      max_steps_ <= 80 && max_steps_ > 0 &&
                      free_probability_ >= 0 && free_probability_ <= 35 &&
                      wall_probability_ >= 65 && behind_wall_ == -1;
    std::ostringstream stream;
    stream << (pass ? "PASS" : "FAIL") << " free=" << static_cast<int>(free_probability_)
           << " wall=" << static_cast<int>(wall_probability_)
           << " behind=" << static_cast<int>(behind_wall_)
           << " received=" << received_ << " processed=" << processed_
           << " rejected=" << rejected_ << " replaced=" << replaced_
           << " max_steps=" << max_steps_;
    std_msgs::msg::String result;
    result.data = stream.str();
    // 결과도 transient_local이므로 수 초 뒤 접속하는 smoke test가 최종 판정을 읽는다.
    result_pub_->publish(result);
    RCLCPP_INFO(get_logger(), "%s", result.data.c_str());
    sent_ = true;
  }

  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr map_sub_;
  rclcpp::Subscription<std_msgs::msg::UInt32MultiArray>::SharedPtr stats_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr result_pub_;
  bool map_seen_{false}, map_contract_ok_{false}, sent_{false};
  std::int8_t free_probability_{-1}, wall_probability_{-1}, behind_wall_{-1};
  std::uint32_t received_{0}, rejected_{0}, processed_{0}, replaced_{0}, max_steps_{0};
};

int main(int argc, char ** argv) {
  // spin은 지도/통계 Topic 콜백을 계속 실행해 최종 검증 결과를 발행한다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MapAuditor>());
  rclcpp::shutdown();
  return 0;
}
