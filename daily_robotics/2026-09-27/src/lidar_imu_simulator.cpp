#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/point_field.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace study
{

constexpr std::size_t kPointCount = 128U;
constexpr std::size_t kPointStep = 20U;
constexpr double kScanDurationSec = 0.1;
constexpr double kPi = 3.14159265358979323846;

struct Vec3
{
  double x{0.0};
  double y{0.0};
  double z{0.0};
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

  Vec3 rotate(const Vec3 & p) const
  {
    // q * [0,p] * q^{-1}: 센서 좌표 벡터를 기준 좌표계로 회전시키는 쿼터니언 식이다.
    const Quaternion result = (*this) * Quaternion{0.0, p.x, p.y, p.z} * conjugate();
    return {result.x, result.y, result.z};
  }
};

Quaternion expQuaternion(const Vec3 & omega, const double seconds)
{
  // Exp(omega*dt): 회전벡터를 단위 쿼터니언으로 옮기는 SO(3) 지수지도이다.
  const double angle = std::sqrt(omega.x * omega.x + omega.y * omega.y + omega.z * omega.z) * seconds;
  if (angle < 1.0e-12) {
    return {};
  }
  const double omega_norm = std::sqrt(
    omega.x * omega.x + omega.y * omega.y + omega.z * omega.z);
  const double scale = std::sin(0.5 * angle) / omega_norm;
  return {std::cos(0.5 * angle), omega.x * scale, omega.y * scale, omega.z * scale};
}

void writeFloat(std::uint8_t * destination, const float value)
{
  // PointCloud2의 data는 바이트 배열이므로 정렬되지 않은 주소에도 안전한 memcpy를 쓴다.
  std::memcpy(destination, &value, sizeof(float));
}

class LidarImuSimulator final : public rclcpp::Node
{
public:
  LidarImuSimulator()
  : Node("lidar_imu_simulator"), start_time_(now())
  {
    // SensorDataQoS는 깊이가 작고 best-effort인 센서 스트림 지향 정책이다.
    // 오래된 IMU를 재전송하느라 최신 제어 입력이 밀리지 않게 한다.
    imu_pub_ = create_publisher<sensor_msgs::msg::Imu>("/study/imu", rclcpp::SensorDataQoS());
    cloud_pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(
      "/study/cloud_raw", rclcpp::SensorDataQoS());

    // 200 Hz IMU는 100 ms LiDAR 스캔마다 약 20개 자세 구간을 제공한다.
    imu_timer_ = create_wall_timer(
      std::chrono::milliseconds(5), std::bind(&LidarImuSimulator::publishImu, this));
    // 10 Hz 회전형 LiDAR 한 바퀴를 단순화하여 100 ms마다 128점을 만든다.
    cloud_timer_ = create_wall_timer(
      std::chrono::milliseconds(100), std::bind(&LidarImuSimulator::publishCloud, this));
  }

private:
  static Vec3 trueAngularRate(const double elapsed_sec)
  {
    // 첫 1초는 정지시켜 deskew 노드가 자이로 바이어스를 관측할 수 있게 한다.
    return elapsed_sec < 1.0 ? Vec3{} : Vec3{0.45, -0.30, 1.10};
  }

  static Quaternion trueOrientation(const double elapsed_sec)
  {
    const double moving_sec = std::max(0.0, elapsed_sec - 1.0);
    return expQuaternion(Vec3{0.45, -0.30, 1.10}, moving_sec);
  }

  double elapsed(const rclcpp::Time & stamp) const
  {
    return (stamp - start_time_).seconds();
  }

  void publishImu()
  {
    sensor_msgs::msg::Imu message;
    const rclcpp::Time stamp = now();
    message.header.stamp = stamp;
    message.header.frame_id = "imu_link";

    const Vec3 omega = trueAngularRate(elapsed(stamp));
    // 센서 고유의 고정 바이어스. deskew 노드는 정지 구간 표본 평균으로 이를 추정한다.
    constexpr Vec3 bias{0.012, -0.008, 0.020};
    message.angular_velocity.x = omega.x + bias.x;
    message.angular_velocity.y = omega.y + bias.y;
    message.angular_velocity.z = omega.z + bias.z;

    // 회전하는 IMU가 보는 중력은 R^T*g이다. 여기서는 특정 힘 -R^T*g 대신
    // 교육용으로 R^T*[0,0,9.81]을 제공하며, deskew는 정지 판정에 norm만 사용한다.
    const Quaternion q_world_sensor = trueOrientation(elapsed(stamp));
    const Vec3 gravity_sensor = q_world_sensor.conjugate().rotate({0.0, 0.0, 9.81});
    message.linear_acceleration.x = gravity_sensor.x;
    message.linear_acceleration.y = gravity_sensor.y;
    message.linear_acceleration.z = gravity_sensor.z;

    // ROS Imu covariance는 행 우선 3x3이다. 대각 2.5e-5는 0.005 rad/s 표준편차를 뜻한다.
    message.angular_velocity_covariance.fill(0.0);
    message.angular_velocity_covariance[0] = 2.5e-5;
    message.angular_velocity_covariance[4] = 2.5e-5;
    message.angular_velocity_covariance[8] = 2.5e-5;
    message.linear_acceleration_covariance.fill(0.0);
    message.linear_acceleration_covariance[0] = 4.0e-4;
    message.linear_acceleration_covariance[4] = 4.0e-4;
    message.linear_acceleration_covariance[8] = 4.0e-4;

    // orientation을 직접 측정하지 않는 IMU 규약: 첫 covariance 원소 -1은 값 미제공을 뜻한다.
    message.orientation_covariance[0] = -1.0;
    imu_pub_->publish(message);
  }

  static void addField(
    sensor_msgs::msg::PointCloud2 & cloud, const std::string & name,
    const std::uint32_t offset)
  {
    sensor_msgs::msg::PointField field;
    field.name = name;
    field.offset = offset;
    field.datatype = sensor_msgs::msg::PointField::FLOAT32;
    field.count = 1U;
    cloud.fields.push_back(field);
  }

  void publishCloud()
  {
    const rclcpp::Time current = now();
    if (elapsed(current) < 1.25) {
      return;  // IMU 바이어스 초기화와 100 ms 과거 구간 확보 후 첫 스캔을 보낸다.
    }

    sensor_msgs::msg::PointCloud2 cloud;
    const rclcpp::Time scan_end = current - rclcpp::Duration::from_seconds(0.015);
    const rclcpp::Time scan_start = scan_end - rclcpp::Duration::from_seconds(kScanDurationSec);
    cloud.header.stamp = scan_start;
    cloud.header.frame_id = "lidar_link";
    cloud.height = 1U;
    cloud.width = static_cast<std::uint32_t>(kPointCount);
    cloud.is_bigendian = false;
    cloud.is_dense = true;
    cloud.point_step = static_cast<std::uint32_t>(kPointStep);
    cloud.row_step = cloud.point_step * cloud.width;
    cloud.fields.reserve(5U);
    addField(cloud, "x", 0U);
    addField(cloud, "y", 4U);
    addField(cloud, "z", 8U);
    addField(cloud, "intensity", 12U);
    // 이 패키지의 명시적 계약: header.stamp는 스캔 시작, time은 점별 상대 초(float32)다.
    addField(cloud, "time", 16U);
    cloud.data.resize(kPointCount * kPointStep);

    for (std::size_t index = 0U; index < kPointCount; ++index) {
      const std::size_t column = index % 16U;
      const std::size_t row = index / 16U;
      const double alpha = static_cast<double>(index) / static_cast<double>(kPointCount - 1U);
      const double point_offset_sec = alpha * kScanDurationSec;
      const rclcpp::Time point_stamp = scan_start + rclcpp::Duration::from_seconds(point_offset_sec);

      // 세계 좌표 x=6 m인 평면을 16x8 격자로 샘플링한다.
      const Vec3 point_world{
        6.0,
        -2.0 + 4.0 * static_cast<double>(column) / 15.0,
        -1.0 + 2.0 * static_cast<double>(row) / 7.0};
      // p_sensor = R_world_sensor^T * p_world: 움직이는 센서가 실제로 관측한 왜곡된 점이다.
      const Quaternion q_world_sensor = trueOrientation(elapsed(point_stamp));
      const Vec3 point_sensor = q_world_sensor.conjugate().rotate(point_world);

      std::uint8_t * const bytes = cloud.data.data() + index * kPointStep;
      writeFloat(bytes + 0U, static_cast<float>(point_sensor.x));
      writeFloat(bytes + 4U, static_cast<float>(point_sensor.y));
      writeFloat(bytes + 8U, static_cast<float>(point_sensor.z));
      writeFloat(bytes + 12U, static_cast<float>(100.0 + static_cast<double>(row)));
      writeFloat(bytes + 16U, static_cast<float>(point_offset_sec));
    }
    cloud_pub_->publish(cloud);
  }

  rclcpp::Time start_time_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr cloud_pub_;
  rclcpp::TimerBase::SharedPtr imu_timer_;
  rclcpp::TimerBase::SharedPtr cloud_timer_;
};

}  // namespace study

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor를 사용하는 spin은 두 타이머 콜백을 직렬화하여 시뮬레이터 상태 경쟁을 없앤다.
  rclcpp::spin(std::make_shared<study::LidarImuSimulator>());
  rclcpp::shutdown();
  return 0;
}
