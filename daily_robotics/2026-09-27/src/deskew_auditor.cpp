#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/point_field.hpp>
#include <std_msgs/msg/bool.hpp>

#include <daily_robotics_2026_09_27/msg/deskew_stats.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

namespace study
{

struct Vec3
{
  double x{0.0};
  double y{0.0};
  double z{0.0};

  Vec3 operator-(const Vec3 & rhs) const {return {x - rhs.x, y - rhs.y, z - rhs.z};}
};

double dot(const Vec3 & lhs, const Vec3 & rhs)
{
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(const Vec3 & lhs, const Vec3 & rhs)
{
  return {
    lhs.y * rhs.z - lhs.z * rhs.y,
    lhs.z * rhs.x - lhs.x * rhs.z,
    lhs.x * rhs.y - lhs.y * rhs.x};
}

float readFloat(const std::uint8_t * source)
{
  float value = 0.0F;
  std::memcpy(&value, source, sizeof(float));
  return value;
}

class DeskewAuditor final : public rclcpp::Node
{
public:
  DeskewAuditor()
  : Node("deskew_auditor")
  {
    // 감사기는 생산 알고리즘과 독립된 노드로 둔다. 잘못된 deskew가 자기 결과를 스스로
    // PASS시키는 공통 실패 모드를 줄이고, 실제 시스템의 safety monitor 경계를 연습한다.
    raw_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      "/study/cloud_raw", rclcpp::SensorDataQoS(),
      std::bind(&DeskewAuditor::onRaw, this, std::placeholders::_1));
    corrected_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      "/study/cloud_deskewed", rclcpp::SensorDataQoS(),
      std::bind(&DeskewAuditor::onCorrected, this, std::placeholders::_1));
    stats_sub_ = create_subscription<daily_robotics_2026_09_27::msg::DeskewStats>(
      "/study/deskew_stats", rclcpp::QoS(10).reliable(),
      std::bind(&DeskewAuditor::onStats, this, std::placeholders::_1));

    // transient_local은 늦게 시작한 smoke test도 마지막 PASS 상태를 받게 하는 latched 유사 QoS다.
    pass_pub_ = create_publisher<std_msgs::msg::Bool>(
      "/study/audit_pass", rclcpp::QoS(1).reliable().transient_local());
  }

private:
  static bool xyzOffsets(
    const sensor_msgs::msg::PointCloud2 & cloud,
    std::array<std::uint32_t, 3U> & offsets)
  {
    std::array<bool, 3U> found{};
    const std::array<std::string, 3U> names{"x", "y", "z"};
    for (const auto & field : cloud.fields) {
      for (std::size_t axis = 0U; axis < names.size(); ++axis) {
        if (field.name == names[axis] && field.datatype == sensor_msgs::msg::PointField::FLOAT32) {
          offsets[axis] = field.offset;
          found[axis] = true;
        }
      }
    }
    return found[0] && found[1] && found[2];
  }

  static Vec3 pointAt(
    const sensor_msgs::msg::PointCloud2 & cloud,
    const std::array<std::uint32_t, 3U> & offsets, const std::size_t index)
  {
    const std::uint8_t * const bytes = cloud.data.data() + index * cloud.point_step;
    return {
      readFloat(bytes + offsets[0]),
      readFloat(bytes + offsets[1]),
      readFloat(bytes + offsets[2])};
  }

  static double planeResidual(const sensor_msgs::msg::PointCloud2 & cloud)
  {
    const std::size_t count = std::min<std::size_t>(
      cloud.width * cloud.height, cloud.data.size() / cloud.point_step);
    if (count < 128U) {
      return 1.0e9;
    }
    std::array<std::uint32_t, 3U> offsets{};
    if (!xyzOffsets(cloud, offsets)) {
      return 1.0e9;
    }

    // 시뮬레이터 격자의 가로 끝(0→15)과 세로 끝(0→112)으로 평면 법선을 독립 계산한다.
    const Vec3 origin = pointAt(cloud, offsets, 0U);
    const Vec3 horizontal = pointAt(cloud, offsets, 15U) - origin;
    const Vec3 vertical = pointAt(cloud, offsets, 112U) - origin;
    Vec3 normal = cross(horizontal, vertical);
    const double norm = std::sqrt(dot(normal, normal));
    if (norm < 1.0e-9) {
      return 1.0e9;
    }
    normal = {normal.x / norm, normal.y / norm, normal.z / norm};

    double absolute_distance_sum = 0.0;
    for (std::size_t i = 0U; i < count; ++i) {
      // |n^T(p-p0)|은 각 점에서 기준 평면까지의 수직거리다.
      absolute_distance_sum += std::abs(dot(normal, pointAt(cloud, offsets, i) - origin));
    }
    return absolute_distance_sum / static_cast<double>(count);
  }

  void onRaw(const sensor_msgs::msg::PointCloud2::SharedPtr message)
  {
    // sensor callback에서 최신 한 장만 보관한다. 감사 대상과 stamp가 맞지 않으면 사용하지 않는다.
    latest_raw_ = *message;
    latest_raw_stamp_ns_ = rclcpp::Time(message->header.stamp).nanoseconds();
  }

  void onStats(const daily_robotics_2026_09_27::msg::DeskewStats::SharedPtr message)
  {
    latest_stats_ = *message;
    have_stats_ = true;
  }

  void onCorrected(const sensor_msgs::msg::PointCloud2::SharedPtr message)
  {
    const std::int64_t stamp_ns = rclcpp::Time(message->header.stamp).nanoseconds();
    if (stamp_ns != latest_raw_stamp_ns_ || !have_stats_) {
      return;
    }

    const double raw_residual = planeResidual(latest_raw_);
    const double corrected_residual = planeResidual(*message);
    constexpr std::array<double, 3U> expected_bias{0.012, -0.008, 0.020};
    double maximum_bias_error = 0.0;
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
      maximum_bias_error = std::max(
        maximum_bias_error,
        std::abs(latest_stats_.gyro_bias_rad_s[axis] - expected_bias[axis]));
    }

    // 품질 계약: 보정 후 평면 오차가 절대 5 mm 미만이며 raw의 20% 미만이어야 한다.
    // 또한 바이어스, 공분산, 고정 입력 크기/샘플 수, 관측 callback 시간을 함께 점검한다.
    const bool pass = corrected_residual < 0.005 &&
      corrected_residual < raw_residual * 0.20 &&
      maximum_bias_error < 0.001 &&
      latest_stats_.rotation_sigma_rad > 0.0 && latest_stats_.rotation_sigma_rad < 0.01 &&
      latest_stats_.output_points == 128U &&
      latest_stats_.imu_samples >= 18U && latest_stats_.imu_samples <= 32U &&
      latest_stats_.truncated_scans == 0U && latest_stats_.callback_us < 5000.0;

    consecutive_passes_ = pass ? consecutive_passes_ + 1U : 0U;
    if (consecutive_passes_ >= 5U && !announced_) {
      announced_ = true;
      RCLCPP_INFO(
        get_logger(),
        "AUDIT_PASS raw=%.6f m deskewed=%.6f m bias_error=%.6f rad/s sigma=%.6f rad "
        "imu=%u callback=%.1f us",
        raw_residual, corrected_residual, maximum_bias_error,
        latest_stats_.rotation_sigma_rad, latest_stats_.imu_samples,
        latest_stats_.callback_us);
      std_msgs::msg::Bool result;
      result.data = true;
      pass_pub_->publish(result);
    }
  }

  sensor_msgs::msg::PointCloud2 latest_raw_{};
  daily_robotics_2026_09_27::msg::DeskewStats latest_stats_{};
  std::int64_t latest_raw_stamp_ns_{std::numeric_limits<std::int64_t>::min()};
  bool have_stats_{false};
  bool announced_{false};
  std::size_t consecutive_passes_{0U};
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr raw_sub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr corrected_sub_;
  rclcpp::Subscription<daily_robotics_2026_09_27::msg::DeskewStats>::SharedPtr stats_sub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_pub_;
};

}  // namespace study

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<study::DeskewAuditor>());
  rclcpp::shutdown();
  return 0;
}
