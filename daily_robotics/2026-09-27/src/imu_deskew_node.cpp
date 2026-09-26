#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/point_field.hpp>

#include <daily_robotics_2026_09_27/msg/deskew_stats.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

namespace study
{

constexpr std::size_t kMaxImuSamples = 512U;
constexpr std::size_t kMaxScanImuSamples = 64U;
constexpr std::size_t kMaxPoints = 128U;
constexpr std::size_t kStateDimension = 6U;
constexpr std::size_t kBiasInitializationSamples = 160U;

struct Vec3
{
  double x{0.0};
  double y{0.0};
  double z{0.0};

  Vec3 operator-(const Vec3 & rhs) const {return {x - rhs.x, y - rhs.y, z - rhs.z};}
  Vec3 operator+(const Vec3 & rhs) const {return {x + rhs.x, y + rhs.y, z + rhs.z};}
  Vec3 operator*(const double scale) const {return {x * scale, y * scale, z * scale};}
};

struct Quaternion
{
  double w{1.0};
  double x{0.0};
  double y{0.0};
  double z{0.0};

  Quaternion conjugate() const {return {w, -x, -y, -z};}

  Quaternion operator*(const Quaternion & rhs) const
  {
    return {
      w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
      w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
      w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
      w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w};
  }

  Quaternion normalized() const
  {
    const double norm = std::sqrt(w * w + x * x + y * y + z * z);
    if (norm < 1.0e-12) {
      return {};
    }
    return {w / norm, x / norm, y / norm, z / norm};
  }

  Vec3 rotate(const Vec3 & vector) const
  {
    // q * [0,p] * q^{-1}: 좌표축을 q만큼 회전시킨 벡터를 얻는다.
    const Quaternion rotated = (*this) * Quaternion{0.0, vector.x, vector.y, vector.z} * conjugate();
    return {rotated.x, rotated.y, rotated.z};
  }
};

Quaternion expQuaternion(const Vec3 & angular_rate, const double dt)
{
  // IMU 각속도 적분 q_{k+1}=q_k Exp((omega-b_g)dt)의 Exp 부분이다.
  const double rate_norm = std::sqrt(
    angular_rate.x * angular_rate.x + angular_rate.y * angular_rate.y +
    angular_rate.z * angular_rate.z);
  const double angle = rate_norm * dt;
  if (angle < 1.0e-12) {
    return {};
  }
  const double scale = std::sin(0.5 * angle) / rate_norm;
  return {
    std::cos(0.5 * angle), angular_rate.x * scale,
    angular_rate.y * scale, angular_rate.z * scale};
}

struct ImuSample
{
  std::int64_t stamp_ns{0};
  Vec3 measured_omega{};
};

struct TimedPoint
{
  Vec3 position{};
  double offset_sec{0.0};
};

using Matrix6 = std::array<std::array<double, kStateDimension>, kStateDimension>;

float readFloat(const std::uint8_t * source)
{
  float value = 0.0F;
  // reinterpret_cast 역참조는 정렬/aliasing 문제가 있으므로 바이트 배열은 memcpy로 읽는다.
  std::memcpy(&value, source, sizeof(float));
  return value;
}

void writeFloat(std::uint8_t * destination, const float value)
{
  std::memcpy(destination, &value, sizeof(float));
}

class ImuDeskewNode final : public rclcpp::Node
{
public:
  ImuDeskewNode()
  : Node("imu_deskew_node")
  {
    // 한 SingleThreadedExecutor에서 두 콜백을 실행하면 IMU ring과 cloud 처리 사이에
    // mutex가 필요 없다. MultiThreadedExecutor로 바꾼다면 callback group 분리만으로는
    // 안전하지 않으므로 명시적 동기화나 SPSC handoff가 추가되어야 한다.
    imu_sub_ = create_subscription<sensor_msgs::msg::Imu>(
      "/study/imu", rclcpp::SensorDataQoS(),
      std::bind(&ImuDeskewNode::onImu, this, std::placeholders::_1));
    cloud_sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      "/study/cloud_raw", rclcpp::SensorDataQoS(),
      std::bind(&ImuDeskewNode::onCloud, this, std::placeholders::_1));
    cloud_pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      "/study/cloud_deskewed", rclcpp::SensorDataQoS());
    stats_pub_ = create_publisher<daily_robotics_2026_09_27::msg::DeskewStats>(
      "/study/deskew_stats", rclcpp::QoS(10).reliable());

    // P=[delta_theta, delta_b_g]의 6x6 오차상태 공분산 초기값이다.
    // orientation은 scan 시작을 원점으로 두므로 작게, bias는 초기 평균의 불확실성을 반영한다.
    for (std::size_t i = 0U; i < 3U; ++i) {
      covariance_[i][i] = 1.0e-8;
      covariance_[i + 3U][i + 3U] = 2.5e-5 / static_cast<double>(kBiasInitializationSamples);
    }
  }

private:
  void onImu(const sensor_msgs::msg::Imu::SharedPtr message)
  {
    const Vec3 omega{
      message->angular_velocity.x,
      message->angular_velocity.y,
      message->angular_velocity.z};

    if (bias_sample_count_ < kBiasInitializationSamples) {
      // 정지 구간에서 E[omega_measured]=b_g라고 보고 산술평균으로 바이어스를 추정한다.
      bias_sum_ = bias_sum_ + omega;
      ++bias_sample_count_;
      if (bias_sample_count_ == kBiasInitializationSamples) {
        gyro_bias_ = bias_sum_ * (1.0 / static_cast<double>(bias_sample_count_));
        RCLCPP_INFO(
          get_logger(), "gyro bias initialized: [%.6f, %.6f, %.6f] rad/s",
          gyro_bias_.x, gyro_bias_.y, gyro_bias_.z);
      }
    }

    // 고정 배열 ring: push 시 heap 할당이 없고, 가장 오래된 표본을 덮어써 메모리가 상한을 가진다.
    imu_ring_[imu_write_index_] = {
      rclcpp::Time(message->header.stamp).nanoseconds(), omega};
    imu_write_index_ = (imu_write_index_ + 1U) % kMaxImuSamples;
    imu_count_ = std::min(imu_count_ + 1U, kMaxImuSamples);
  }

  static bool findField(
    const sensor_msgs::msg::PointCloud2 & cloud, const std::string & name,
    std::uint32_t & offset)
  {
    for (const auto & field : cloud.fields) {
      if (field.name == name && field.datatype == sensor_msgs::msg::PointField::FLOAT32 &&
        field.count == 1U)
      {
        offset = field.offset;
        return true;
      }
    }
    return false;
  }

  std::size_t copyRelevantImu(
    const std::int64_t scan_start_ns, const std::int64_t scan_end_ns,
    std::array<ImuSample, kMaxScanImuSamples> & selected) const
  {
    std::size_t output_count = 0U;
    const std::size_t oldest = (imu_write_index_ + kMaxImuSamples - imu_count_) % kMaxImuSamples;
    constexpr std::int64_t margin_ns = 10'000'000LL;
    for (std::size_t i = 0U; i < imu_count_; ++i) {
      const ImuSample & sample = imu_ring_[(oldest + i) % kMaxImuSamples];
      if (sample.stamp_ns >= scan_start_ns - margin_ns &&
        sample.stamp_ns <= scan_end_ns + margin_ns)
      {
        if (output_count == kMaxScanImuSamples) {
          break;
        }
        selected[output_count++] = sample;
      }
    }
    return output_count;
  }

  Quaternion orientationAt(
    const std::array<ImuSample, kMaxScanImuSamples> & samples,
    const std::size_t sample_count, const std::int64_t target_ns) const
  {
    Quaternion orientation{};
    if (sample_count < 2U || target_ns <= samples[0].stamp_ns) {
      return orientation;
    }

    // 최대 63구간만 순회한다. 각 점마다 선형 탐색하더라도 상한은
    // 128 points * 63 intervals로 명시되어 입력 폭주가 실행시간을 무한히 늘리지 않는다.
    for (std::size_t i = 0U; i + 1U < sample_count; ++i) {
      const std::int64_t interval_start = samples[i].stamp_ns;
      const std::int64_t interval_end = samples[i + 1U].stamp_ns;
      if (target_ns <= interval_start) {
        break;
      }
      const std::int64_t used_end = std::min(target_ns, interval_end);
      const double dt = static_cast<double>(used_end - interval_start) * 1.0e-9;
      if (dt > 0.0) {
        // 사다리꼴 적분은 양 끝 각속도 평균을 써 zero-order hold보다 시간 중심에 가깝다.
        const Vec3 corrected_left = samples[i].measured_omega - gyro_bias_;
        const Vec3 corrected_right = samples[i + 1U].measured_omega - gyro_bias_;
        const Vec3 midpoint_rate = (corrected_left + corrected_right) * 0.5;
        orientation = (orientation * expQuaternion(midpoint_rate, dt)).normalized();
      }
      if (target_ns <= interval_end) {
        break;
      }
    }
    return orientation;
  }

  void propagateCovariance(
    const std::array<ImuSample, kMaxScanImuSamples> & samples,
    const std::size_t sample_count, const std::int64_t start_ns,
    const std::int64_t end_ns)
  {
    covariance_ = {};
    for (std::size_t i = 0U; i < 3U; ++i) {
      covariance_[i][i] = 1.0e-8;
      covariance_[i + 3U][i + 3U] = 2.5e-5 / static_cast<double>(kBiasInitializationSamples);
    }

    constexpr double gyro_noise_density = 0.005;  // rad/s/sqrt(Hz), 교육용 센서 사양
    constexpr double bias_random_walk = 0.0002;   // rad/s^2/sqrt(Hz)
    for (std::size_t k = 0U; k + 1U < sample_count; ++k) {
      const std::int64_t clipped_start = std::max(start_ns, samples[k].stamp_ns);
      const std::int64_t clipped_end = std::min(end_ns, samples[k + 1U].stamp_ns);
      const double dt = static_cast<double>(clipped_end - clipped_start) * 1.0e-9;
      if (dt <= 0.0) {
        continue;
      }

      Matrix6 transition{};
      for (std::size_t i = 0U; i < kStateDimension; ++i) {
        transition[i][i] = 1.0;
      }
      // 선형화한 dtheta_{k+1}=dtheta_k-dbias*dt+n_g 에서 F(theta,bias)=-I*dt다.
      for (std::size_t axis = 0U; axis < 3U; ++axis) {
        transition[axis][axis + 3U] = -dt;
      }

      Matrix6 temporary{};
      Matrix6 propagated{};
      for (std::size_t row = 0U; row < kStateDimension; ++row) {
        for (std::size_t column = 0U; column < kStateDimension; ++column) {
          for (std::size_t inner = 0U; inner < kStateDimension; ++inner) {
            temporary[row][column] += transition[row][inner] * covariance_[inner][column];
          }
        }
      }
      // P_{k+1}=F P F^T + Q: 센서 노이즈와 bias random walk를 시간에 따라 누적한다.
      for (std::size_t row = 0U; row < kStateDimension; ++row) {
        for (std::size_t column = 0U; column < kStateDimension; ++column) {
          for (std::size_t inner = 0U; inner < kStateDimension; ++inner) {
            propagated[row][column] += temporary[row][inner] * transition[column][inner];
          }
        }
      }
      for (std::size_t axis = 0U; axis < 3U; ++axis) {
        propagated[axis][axis] += gyro_noise_density * gyro_noise_density * dt;
        propagated[axis + 3U][axis + 3U] += bias_random_walk * bias_random_walk * dt;
      }
      covariance_ = propagated;
    }
  }

  void reject(const sensor_msgs::msg::PointCloud2 & cloud, const char * reason)
  {
    ++rejected_scans_;
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "scan rejected: %s", reason);
    daily_robotics_2026_09_27::msg::DeskewStats stats;
    stats.header = cloud.header;
    stats.input_points = cloud.width * cloud.height;
    stats.rejected_scans = rejected_scans_;
    stats.truncated_scans = truncated_scans_;
    stats.gyro_bias_rad_s = {gyro_bias_.x, gyro_bias_.y, gyro_bias_.z};
    stats_pub_->publish(stats);
  }

  void onCloud(const sensor_msgs::msg::PointCloud2::SharedPtr message)
  {
    const auto callback_start = std::chrono::steady_clock::now();
    if (bias_sample_count_ < kBiasInitializationSamples) {
      reject(*message, "gyro bias is not initialized");
      return;
    }

    std::uint32_t x_offset = 0U;
    std::uint32_t y_offset = 0U;
    std::uint32_t z_offset = 0U;
    std::uint32_t time_offset = 0U;
    if (message->is_bigendian || message->height != 1U ||
      !findField(*message, "x", x_offset) || !findField(*message, "y", y_offset) ||
      !findField(*message, "z", z_offset) || !findField(*message, "time", time_offset) ||
      message->point_step < 20U)
    {
      reject(*message, "PointCloud2 must be little-endian, unorganized, and contain float32 x/y/z/time");
      return;
    }

    const std::size_t available_points = std::min<std::size_t>(
      message->width, message->data.size() / message->point_step);
    const std::size_t point_count = std::min(available_points, kMaxPoints);
    if (point_count == 0U) {
      reject(*message, "empty cloud");
      return;
    }
    if (available_points > kMaxPoints) {
      ++truncated_scans_;
    }

    std::array<TimedPoint, kMaxPoints> points{};
    double maximum_offset_sec = 0.0;
    for (std::size_t i = 0U; i < point_count; ++i) {
      const std::uint8_t * const bytes = message->data.data() + i * message->point_step;
      points[i] = {
        {readFloat(bytes + x_offset), readFloat(bytes + y_offset), readFloat(bytes + z_offset)},
        static_cast<double>(readFloat(bytes + time_offset))};
      if (!std::isfinite(points[i].offset_sec) || points[i].offset_sec < 0.0 ||
        points[i].offset_sec > 0.2)
      {
        reject(*message, "time field is outside [0, 0.2] seconds");
        return;
      }
      maximum_offset_sec = std::max(maximum_offset_sec, points[i].offset_sec);
    }

    const std::int64_t scan_start_ns = rclcpp::Time(message->header.stamp).nanoseconds();
    const std::int64_t scan_end_ns = scan_start_ns +
      static_cast<std::int64_t>(maximum_offset_sec * 1.0e9);
    std::array<ImuSample, kMaxScanImuSamples> scan_imu{};
    const std::size_t imu_sample_count = copyRelevantImu(scan_start_ns, scan_end_ns, scan_imu);
    if (imu_sample_count < 2U || scan_imu[0].stamp_ns > scan_start_ns ||
      scan_imu[imu_sample_count - 1U].stamp_ns < scan_end_ns)
    {
      reject(*message, "IMU buffer does not bracket the complete scan");
      return;
    }

    const Quaternion orientation_end = orientationAt(scan_imu, imu_sample_count, scan_end_ns);
    sensor_msgs::msg::PointCloud2 output = *message;
    output.header.frame_id = "lidar_link_at_scan_end";
    output.width = static_cast<std::uint32_t>(point_count);
    output.row_step = output.width * output.point_step;
    output.data.resize(static_cast<std::size_t>(output.row_step));

    for (std::size_t i = 0U; i < point_count; ++i) {
      const std::int64_t point_ns = scan_start_ns +
        static_cast<std::int64_t>(points[i].offset_sec * 1.0e9);
      const Quaternion orientation_point = orientationAt(scan_imu, imu_sample_count, point_ns);
      // p_end = R_end^T R_i p_i: 서로 다른 취득 시각의 점을 scan-end 센서 좌표로 모은다.
      const Vec3 corrected = (orientation_end.conjugate() * orientation_point).rotate(points[i].position);
      std::uint8_t * const bytes = output.data.data() + i * output.point_step;
      writeFloat(bytes + x_offset, static_cast<float>(corrected.x));
      writeFloat(bytes + y_offset, static_cast<float>(corrected.y));
      writeFloat(bytes + z_offset, static_cast<float>(corrected.z));
    }

    propagateCovariance(scan_imu, imu_sample_count, scan_start_ns, scan_end_ns);
    const double rotation_sigma = std::sqrt(std::max(
      0.0, (covariance_[0][0] + covariance_[1][1] + covariance_[2][2]) / 3.0));
    const auto callback_end = std::chrono::steady_clock::now();
    const double callback_us = std::chrono::duration<double, std::micro>(
      callback_end - callback_start).count();

    cloud_pub_->publish(output);
    daily_robotics_2026_09_27::msg::DeskewStats stats;
    stats.header = output.header;
    stats.input_points = static_cast<std::uint32_t>(available_points);
    stats.output_points = static_cast<std::uint32_t>(point_count);
    stats.imu_samples = static_cast<std::uint32_t>(imu_sample_count);
    stats.rejected_scans = rejected_scans_;
    stats.truncated_scans = truncated_scans_;
    stats.gyro_bias_rad_s = {gyro_bias_.x, gyro_bias_.y, gyro_bias_.z};
    stats.rotation_sigma_rad = rotation_sigma;
    stats.callback_us = callback_us;
    stats_pub_->publish(stats);
  }

  std::array<ImuSample, kMaxImuSamples> imu_ring_{};
  std::size_t imu_write_index_{0U};
  std::size_t imu_count_{0U};
  std::size_t bias_sample_count_{0U};
  Vec3 bias_sum_{};
  Vec3 gyro_bias_{};
  Matrix6 covariance_{};
  std::uint32_t rejected_scans_{0U};
  std::uint32_t truncated_scans_{0U};

  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_sub_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_pub_;
  rclcpp::Publisher<daily_robotics_2026_09_27::msg::DeskewStats>::SharedPtr stats_pub_;
};

}  // namespace study

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // rclcpp::spin은 DDS wait set을 기다리며 준비된 subscription 콜백을 실행한다.
  // 이 예제는 단일 스레드 직렬 실행을 설계 계약으로 삼는다.
  rclcpp::spin(std::make_shared<study::ImuDeskewNode>());
  rclcpp::shutdown();
  return 0;
}
