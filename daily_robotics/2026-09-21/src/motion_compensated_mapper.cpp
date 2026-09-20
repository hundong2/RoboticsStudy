#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "nav_msgs/msg/occupancy_grid.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"
#include "tf2/exceptions.h"
#include "tf2/time.h"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"

namespace {
constexpr std::size_t kMaxBeams = 181;
constexpr int kWidth = 100;
constexpr int kHeight = 100;
constexpr int kCellCount = kWidth * kHeight;
constexpr int kMaxRaySteps = 100;
constexpr double kResolution = 0.10;
constexpr double kRayStep = 0.08;
constexpr double kOriginX = 0.0;
constexpr double kOriginY = -5.0;

struct Pose2d {
  double x;
  double y;
  double yaw;
};

// 쿼터니언에서 planar yaw를 복원한다:
// yaw=atan2(2(wz+xy), 1-2(y²+z²)).
double yaw_from(const geometry_msgs::msg::Quaternion & q) {
  const double sin_yaw = 2.0 * (q.w * q.z + q.x * q.y);
  const double cos_yaw = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
  return std::atan2(sin_yaw, cos_yaw);
}

Pose2d pose_from(const geometry_msgs::msg::TransformStamped & transform) {
  return Pose2d{transform.transform.translation.x,
                transform.transform.translation.y,
                yaw_from(transform.transform.rotation)};
}

// [-pi,pi]의 최단 각도 차를 사용해야 +179°→-179° 보간이 358° 역회전하지 않는다.
double shortest_angle(double angle) {
  return std::atan2(std::sin(angle), std::cos(angle));
}

Pose2d interpolate(const Pose2d & start, const Pose2d & end, double alpha) {
  return Pose2d{
    start.x + alpha * (end.x - start.x),
    start.y + alpha * (end.y - start.y),
    start.yaw + alpha * shortest_angle(end.yaw - start.yaw)};
}

int cell_index(double x, double y) {
  const int col = static_cast<int>(std::floor((x - kOriginX) / kResolution));
  const int row = static_cast<int>(std::floor((y - kOriginY) / kResolution));
  if (col < 0 || col >= kWidth || row < 0 || row >= kHeight) {
    return -1;
  }
  return row * kWidth + col;
}
}  // namespace

// /scan과 TF를 결합해 raw/deskew 지도를 동시에 만든다.
// 로봇 시스템에서의 역할은 "측정 시각이 다른 라이다 광선을 공통 map 시각축으로 정렬"하는 것이다.
class MotionCompensatedMapper final : public rclcpp::Node {
 public:
  MotionCompensatedMapper()
  : Node("motion_compensated_mapper"),
    // Buffer는 TF 시간 캐시이며, TransformListener는 /tf와 /tf_static을 자동 구독해 채운다.
    tf_buffer_(get_clock()), tf_listener_(tf_buffer_) {
    scan_sub_ = create_subscription<sensor_msgs::msg::LaserScan>(
        "/scan", rclcpp::SensorDataQoS(),
        [this](sensor_msgs::msg::LaserScan::ConstSharedPtr scan) { on_scan(*scan); });

    // 지도는 전체 상태 스냅샷이므로 reliable+transient_local로 마지막 값을 늦은 구독자에게 재전달한다.
    const auto snapshot_qos = rclcpp::QoS(1).reliable().transient_local();
    raw_map_pub_ = create_publisher<nav_msgs::msg::OccupancyGrid>("/mapping/raw", snapshot_qos);
    deskew_map_pub_ = create_publisher<nav_msgs::msg::OccupancyGrid>("/mapping/deskewed", snapshot_qos);
    stats_pub_ = create_publisher<std_msgs::msg::Float64MultiArray>("/mapping/stats", snapshot_qos);
  }

 private:
  // LaserScan 크기/각도/시간/프레임 계약을 한 곳에서 검증한다.
  bool valid_contract(const sensor_msgs::msg::LaserScan & scan) const {
    if (scan.header.frame_id.empty() || scan.ranges.empty() || scan.ranges.size() > kMaxBeams ||
        !std::isfinite(scan.angle_min) || !std::isfinite(scan.angle_max) ||
        !std::isfinite(scan.angle_increment) || scan.angle_increment <= 0.0F ||
        !std::isfinite(scan.time_increment) || scan.time_increment <= 0.0F ||
        !std::isfinite(scan.range_min) || !std::isfinite(scan.range_max) ||
        scan.range_min <= 0.0F || scan.range_min >= scan.range_max || scan.range_max > 8.0F) {
      return false;
    }
    const double expected_max = static_cast<double>(scan.angle_min) +
      static_cast<double>(scan.ranges.size() - 1) * static_cast<double>(scan.angle_increment);
    return std::abs(expected_max - static_cast<double>(scan.angle_max)) <= 0.02;
  }

  // 로그 오즈에 센서 증거를 더한다. confidence는 pose 불확실도가 큰 점유 끝점을 약하게 만든다.
  void add_evidence(std::array<float, kCellCount> & log_odds,
                    std::array<bool, kCellCount> & observed,
                    int index, float increment) {
    if (index < 0) {
      return;
    }
    const auto cell = static_cast<std::size_t>(index);
    observed[cell] = true;
    log_odds[cell] = std::clamp(log_odds[cell] + increment, -4.0F, 4.0F);
  }

  // 한 광선에서 origin→endpoint 앞은 free, 반사 endpoint는 occupied로 갱신한다.
  // 루프 상한은 min(ceil(range/8 cm),100)이어서 입력 하나당 계산량이 명시적으로 제한된다.
  void integrate_ray(std::array<float, kCellCount> & log_odds,
                     std::array<bool, kCellCount> & observed,
                     const Pose2d & laser_pose, double local_angle, double range,
                     float occupied_confidence) {
    const double world_angle = laser_pose.yaw + local_angle;
    const double direction_x = std::cos(world_angle);
    const double direction_y = std::sin(world_angle);
    const double free_limit = std::max(0.0, range - kResolution);
    const int step_limit = std::min(kMaxRaySteps,
      static_cast<int>(std::ceil(free_limit / kRayStep)));
    int previous_cell = -1;
    for (int step = 1; step <= step_limit; ++step) {
      const double distance = std::min(free_limit, static_cast<double>(step) * kRayStep);
      const int index = cell_index(laser_pose.x + distance * direction_x,
                                   laser_pose.y + distance * direction_y);
      if (index < 0) {
        break;
      }
      // 8 cm 샘플 두 개가 같은 10 cm 셀에 들면 광선당 한 번만 free 증거를 준다.
      if (index != previous_cell) {
        add_evidence(log_odds, observed, index, -0.25F);
      }
      previous_cell = index;
      max_ray_steps_seen_ = std::max(max_ray_steps_seen_, static_cast<std::uint32_t>(step));
    }
    const int endpoint = cell_index(laser_pose.x + range * direction_x,
                                    laser_pose.y + range * direction_y);
    add_evidence(log_odds, observed, endpoint, 0.85F * occupied_confidence);
  }

  // base_link 포즈와 base_link→laser 외부 파라미터를 SE(2) 합성한다:
  // t_map_laser=t_map_base+R(yaw_base)t_base_laser, yaw는 합한다.
  Pose2d laser_pose(const Pose2d & base_pose, const Pose2d & base_to_laser) const {
    return Pose2d{
      base_pose.x + std::cos(base_pose.yaw) * base_to_laser.x -
        std::sin(base_pose.yaw) * base_to_laser.y,
      base_pose.y + std::sin(base_pose.yaw) * base_to_laser.x +
        std::cos(base_pose.yaw) * base_to_laser.y,
      base_pose.yaw + base_to_laser.yaw};
  }

  // 고정 크기 내부 배열을 ROS OccupancyGrid의 row-major -1/0..100 계약으로 변환한다.
  nav_msgs::msg::OccupancyGrid make_map(
      const std::array<float, kCellCount> & log_odds,
      const std::array<bool, kCellCount> & observed,
      const builtin_interfaces::msg::Time & stamp) const {
    nav_msgs::msg::OccupancyGrid map;
    map.header.stamp = stamp;
    map.header.frame_id = "map";
    map.info.map_load_time = stamp;
    map.info.resolution = static_cast<float>(kResolution);
    map.info.width = kWidth;
    map.info.height = kHeight;
    map.info.origin.position.x = kOriginX;
    map.info.origin.position.y = kOriginY;
    map.info.origin.orientation.w = 1.0;
    map.data.resize(kCellCount);
    for (std::size_t i = 0; i < map.data.size(); ++i) {
      if (!observed[i]) {
        map.data[i] = -1;
      } else {
        // p=1/(1+exp(-l)): 가산 로그 오즈를 OccupancyGrid 정수 확률로 되돌린다.
        const float probability = 1.0F / (1.0F + std::exp(-log_odds[i]));
        map.data[i] = static_cast<std::int8_t>(std::lround(100.0F * probability));
      }
    }
    return map;
  }

  // 한 scan 콜백은 TF 3회 조회와 최대 181×2×100 셀 샘플로 상한이 정해져 있다.
  void on_scan(const sensor_msgs::msg::LaserScan & scan) {
    const auto callback_start = std::chrono::steady_clock::now();
    ++received_scans_;
    if (!valid_contract(scan)) {
      ++rejected_scans_;
      return;
    }

    const rclcpp::Time start_stamp(scan.header.stamp);
    const double duration = static_cast<double>(scan.time_increment) *
      static_cast<double>(scan.ranges.size() - 1);
    const rclcpp::Time end_stamp = start_stamp + rclcpp::Duration::from_seconds(duration);

    Pose2d start_base{};
    Pose2d end_base{};
    Pose2d base_to_laser{};
    try {
      // exact-time lookup은 최신 TF가 아니라 각 스캔의 실제 시작/끝 시각을 요청한다.
      // 5 ms timeout은 누락 TF 때문에 센서 콜백이 무한히 막히지 않도록 정한 예산이다.
      const auto timeout = rclcpp::Duration::from_seconds(0.005);
      start_base = pose_from(tf_buffer_.lookupTransform("map", "base_link", start_stamp, timeout));
      end_base = pose_from(tf_buffer_.lookupTransform("map", "base_link", end_stamp, timeout));
      // TimePointZero는 가장 최신 static transform을 뜻한다. static이므로 측정 시각과 무관하다.
      base_to_laser = pose_from(
        tf_buffer_.lookupTransform("base_link", scan.header.frame_id, tf2::TimePointZero));
    } catch (const tf2::TransformException & error) {
      ++tf_failures_;
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
                           "TF contract not ready: %s", error.what());
      return;
    }

    const Pose2d raw_laser_pose = laser_pose(start_base, base_to_laser);
    for (std::size_t i = 0; i < scan.ranges.size(); ++i) {
      const double range = static_cast<double>(scan.ranges[i]);
      // Inf/NaN, range_min 밖, range_max 이상은 실제 반사 끝점이 아니므로 이 데모에서 버린다.
      if (!std::isfinite(range) || range < scan.range_min || range >= scan.range_max) {
        continue;
      }
      const double alpha = scan.ranges.size() == 1 ? 0.0 :
        static_cast<double>(i) / static_cast<double>(scan.ranges.size() - 1);
      const double local_angle = static_cast<double>(scan.angle_min) +
        static_cast<double>(i) * static_cast<double>(scan.angle_increment);

      // raw 지도는 모든 광선을 첫 시각 포즈에 투영한다. rolling scan 왜곡의 비교 기준이다.
      integrate_ray(raw_log_odds_, raw_observed_, raw_laser_pose, local_angle, range, 1.0F);

      // T_i = interp(T_start,T_end,alpha): 두 TF만 조회하고 181개 포즈는 고정 비용 산술로 만든다.
      const Pose2d corrected_base = interpolate(start_base, end_base, alpha);
      const Pose2d corrected_laser = laser_pose(corrected_base, base_to_laser);

      // 간단한 endpoint 불확실도 모델:
      // sigma_end²=sigma_xy²+(range*sigma_yaw)²+sigma_range².
      // 보간 중앙(alpha=0.5)에서 모델 오차가 가장 크다고 보고 점유 증거를 약하게 한다.
      const double middle = 4.0 * alpha * (1.0 - alpha);
      const double sigma_xy = 0.008 + 0.018 * middle;
      const double sigma_yaw = 0.002 + 0.008 * middle;
      const double sigma_range = 0.005;
      const double sigma_endpoint = std::sqrt(
        sigma_xy * sigma_xy + (range * sigma_yaw) * (range * sigma_yaw) +
        sigma_range * sigma_range);
      max_sigma_m_ = std::max(max_sigma_m_, sigma_endpoint);
      // w=1/(1+(sigma/resolution)^2)는 휴리스틱 신뢰도다. 완전한 확률 필터가 아니다.
      const float confidence = static_cast<float>(std::clamp(
        1.0 / (1.0 + std::pow(sigma_endpoint / kResolution, 2.0)), 0.25, 1.0));
      integrate_ray(deskew_log_odds_, deskew_observed_, corrected_laser,
                    local_angle, range, confidence);
      ++accepted_beams_;
    }

    ++processed_scans_;
    const auto raw_map = make_map(raw_log_odds_, raw_observed_, scan.header.stamp);
    const auto deskew_map = make_map(deskew_log_odds_, deskew_observed_, scan.header.stamp);
    raw_map_pub_->publish(raw_map);
    deskew_map_pub_->publish(deskew_map);

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - callback_start).count();
    max_callback_us_ = std::max(max_callback_us_, static_cast<std::uint64_t>(elapsed));

    std_msgs::msg::Float64MultiArray stats;
    // 순서 계약: received,rejected,processed,tf_failures,accepted_beams,
    // max_beams,max_ray_steps,max_callback_us,max_sigma_m.
    stats.data = {static_cast<double>(received_scans_), static_cast<double>(rejected_scans_),
                  static_cast<double>(processed_scans_), static_cast<double>(tf_failures_),
                  static_cast<double>(accepted_beams_), static_cast<double>(scan.ranges.size()),
                  static_cast<double>(max_ray_steps_seen_), static_cast<double>(max_callback_us_),
                  max_sigma_m_};
    stats_pub_->publish(stats);
  }

  tf2_ros::Buffer tf_buffer_;
  tf2_ros::TransformListener tf_listener_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr raw_map_pub_;
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr deskew_map_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr stats_pub_;
  std::array<float, kCellCount> raw_log_odds_{};
  std::array<float, kCellCount> deskew_log_odds_{};
  std::array<bool, kCellCount> raw_observed_{};
  std::array<bool, kCellCount> deskew_observed_{};
  std::uint32_t received_scans_{0};
  std::uint32_t rejected_scans_{0};
  std::uint32_t processed_scans_{0};
  std::uint32_t tf_failures_{0};
  std::uint64_t accepted_beams_{0};
  std::uint64_t max_callback_us_{0};
  std::uint32_t max_ray_steps_seen_{0};
  double max_sigma_m_{0.0};
};

int main(int argc, char ** argv) {
  // SingleThreadedExecutor인 rclcpp::spin은 scan 콜백을 직렬 실행해 지도 배열에 별도 mutex가 필요 없다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MotionCompensatedMapper>());
  rclcpp::shutdown();
  return 0;
}
