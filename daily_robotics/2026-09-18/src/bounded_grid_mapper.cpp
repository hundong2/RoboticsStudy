#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "nav_msgs/msg/occupancy_grid.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "std_msgs/msg/u_int32_multi_array.hpp"

namespace {
constexpr std::size_t kMaxBeams = 181;
constexpr int kWidth = 80;
constexpr int kHeight = 80;
constexpr int kMaxSteps = 80;  // range_max=4 m / ray step=0.05 m.
constexpr float kResolution = 0.1F;
constexpr float kRayStep = 0.05F;
constexpr float kOrigin = -4.0F;

// 지도 좌표를 row-major 셀 인덱스로 바꾼다: index=floor((y-y0)/res)*width+floor((x-x0)/res).
int cell_index(float x, float y) {
  const int col = static_cast<int>(std::floor((x - kOrigin) / kResolution));
  const int row = static_cast<int>(std::floor((y - kOrigin) / kResolution));
  if (col < 0 || col >= kWidth || row < 0 || row >= kHeight) { return -1; }
  return row * kWidth + col;
}
}  // namespace

// /scan을 고정 배열에 받고, 처리 타이머에서 점유 격자와 작업량 통계를 발행한다.
class BoundedGridMapper final : public rclcpp::Node {
 public:
  BoundedGridMapper() : Node("bounded_grid_mapper") {
    // 센서 구독에는 송신 노드와 같은 SensorDataQoS를 써서 DDS QoS를 호환시킨다.
    scan_sub_ = create_subscription<sensor_msgs::msg::LaserScan>(
        "/scan", rclcpp::SensorDataQoS(),
        [this](sensor_msgs::msg::LaserScan::ConstSharedPtr msg) { on_scan(*msg); });
    // transient_local은 늦게 구독한 감사 노드에도 최신 전체 지도를 재전달한다.
    const auto snapshot_qos = rclcpp::QoS(1).reliable().transient_local();
    map_pub_ = create_publisher<nav_msgs::msg::OccupancyGrid>("/map", snapshot_qos);
    stats_pub_ = create_publisher<std_msgs::msg::UInt32MultiArray>("/map/stats", snapshot_qos);
    // 콜백과 타이머는 기본 단일 스레드 executor에서 직렬 실행된다. 별도 락은 필요 없다.
    timer_ = create_wall_timer(std::chrono::milliseconds(200), [this]() { process_latest(); });
  }

 private:
  // 센서 콜백은 크기/좌표계 계약을 확인하고 최대 181개 float만 복사한다.
  void on_scan(const sensor_msgs::msg::LaserScan & scan) {
    ++received_;
    if (scan.header.frame_id != "map" || scan.ranges.empty() ||
        scan.ranges.size() > kMaxBeams || !std::isfinite(scan.angle_min) ||
        !std::isfinite(scan.angle_max) || !std::isfinite(scan.angle_increment) ||
        scan.angle_increment <= 0.0F ||
        std::abs(scan.angle_min + static_cast<float>(scan.ranges.size() - 1) *
                 scan.angle_increment - scan.angle_max) > 0.02F ||
        !std::isfinite(scan.range_min) || !std::isfinite(scan.range_max) ||
        scan.range_min <= 0.0F || scan.range_max > 4.0F ||
        scan.range_min >= scan.range_max) {
      ++rejected_scans_;
      return;
    }
    if (pending_) { ++replaced_scans_; }  // 처리보다 새 샘플이 빠르면 최신 스캔만 보존한다.
    beam_count_ = scan.ranges.size();
    std::copy_n(scan.ranges.begin(), beam_count_, ranges_.begin());
    angle_min_ = scan.angle_min;
    angle_increment_ = scan.angle_increment;
    range_min_ = scan.range_min;
    range_max_ = scan.range_max;
    latest_stamp_ = scan.header.stamp;
    pending_ = true;
  }

  // 로그 오즈 l=log(p/(1-p))에 독립 관측의 근사 증분을 더하고 [-4,4]로 포화한다.
  void add_evidence(int index, float increment) {
    if (index < 0) { return; }
    const auto cell = static_cast<std::size_t>(index);
    observed_[cell] = true;
    log_odds_[cell] = std::clamp(log_odds_[cell] + increment, -4.0F, 4.0F);
  }

  // 유효 광선 하나로 원점부터 끝점 앞까지 free, 실제 반사 끝점만 occupied로 누적한다.
  void integrate_ray(std::size_t i) {
    const float range = ranges_[i];
    // LaserScan 계약에 따라 NaN/Inf와 범위 밖 값은 관측으로 쓰지 않는다.
    if (!std::isfinite(range) || range < range_min_ || range > range_max_) { return; }
    const float theta = angle_min_ + static_cast<float>(i) * angle_increment_;
    const float c = std::cos(theta);
    const float s = std::sin(theta);
    // 데모의 range==range_max는 반사 없음: 끝점 점유 증거 없이 free 영역만 그린다.
    const bool hit = range < range_max_ - 1.0e-4F;
    const float free_limit = hit ? std::max(0.0F, range - kResolution) : range;
    int previous_cell = -1;
    for (int step = 1; step <= kMaxSteps; ++step) {
      const float distance = static_cast<float>(step) * kRayStep;
      if (distance >= free_limit) { break; }
      const int index = cell_index(distance * c, distance * s);
      if (index < 0) { break; }
      // 같은 광선의 두 5 cm 샘플이 같은 10 cm 셀을 중복 갱신하지 않게 한다.
      if (index != previous_cell) { add_evidence(index, -0.4F); }
      previous_cell = index;
      max_steps_seen_ = std::max(max_steps_seen_, static_cast<std::uint32_t>(step));
    }
    if (hit) { add_evidence(cell_index(range * c, range * s), +0.85F); }
  }

  // 최대 181×80 샘플을 처리한 뒤 ROS OccupancyGrid 계약에 맞춰 스냅샷을 만든다.
  void process_latest() {
    if (!pending_) { return; }
    pending_ = false;
    for (std::size_t i = 0; i < beam_count_; ++i) { integrate_ray(i); }
    ++processed_scans_;

    nav_msgs::msg::OccupancyGrid map;
    map.header.stamp = latest_stamp_;
    map.header.frame_id = "map";
    map.info.map_load_time = latest_stamp_;
    map.info.resolution = kResolution;
    map.info.width = kWidth;
    map.info.height = kHeight;
    map.info.origin.position.x = kOrigin;
    map.info.origin.position.y = kOrigin;
    map.info.origin.orientation.w = 1.0;  // 회전 없는 지도 원점의 단위 쿼터니언.
    map.data.resize(kWidth * kHeight);
    for (std::size_t i = 0; i < map.data.size(); ++i) {
      if (!observed_[i]) {
        map.data[i] = -1;  // 관측 전은 p=0.5가 아니라 ROS의 unknown 값.
      } else {
        // p=1/(1+exp(-l)): 내부 로그 오즈를 OccupancyGrid의 0~100 확률로 변환한다.
        const float probability = 1.0F / (1.0F + std::exp(-log_odds_[i]));
        map.data[i] = static_cast<std::int8_t>(std::lround(probability * 100.0F));
      }
    }
    // publish와 벡터 resize는 동적 할당/미들웨어 작업일 수 있으므로 하드 RT 경로가 아니다.
    map_pub_->publish(map);

    std_msgs::msg::UInt32MultiArray stats;
    // 순서: 수신, 계약 거절, 처리, 최신값 교체, 한 광선의 최대 5 cm 스텝.
    stats.data = {received_, rejected_scans_, processed_scans_, replaced_scans_, max_steps_seen_};
    stats_pub_->publish(stats);
  }

  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr map_pub_;
  rclcpp::Publisher<std_msgs::msg::UInt32MultiArray>::SharedPtr stats_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::array<float, kMaxBeams> ranges_{};
  std::array<float, kWidth * kHeight> log_odds_{};
  std::array<bool, kWidth * kHeight> observed_{};
  std::size_t beam_count_{0};
  float angle_min_{0.0F}, angle_increment_{0.0F}, range_min_{0.0F}, range_max_{0.0F};
  builtin_interfaces::msg::Time latest_stamp_{};
  bool pending_{false};
  std::uint32_t received_{0}, rejected_scans_{0}, processed_scans_{0}, replaced_scans_{0};
  std::uint32_t max_steps_seen_{0};
};

int main(int argc, char ** argv) {
  // 단일 스레드 spin이 구독 콜백과 처리 타이머를 순서대로 실행한다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BoundedGridMapper>());
  rclcpp::shutdown();
  return 0;
}
