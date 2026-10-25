#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "daily_robotics_2026_10_26/msg/stereo_correspondences.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/camera_info.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr std::size_t kMatchCount = 48U;
constexpr std::uint32_t kWidth = 320U;
constexpr std::uint32_t kHeight = 240U;
constexpr double kFxPx = 180.0;
constexpr double kFyPx = 180.0;
constexpr double kCxPx = 159.5;
constexpr double kCyPx = 119.5;
constexpr double kBaselineM = 0.12;
constexpr double kForwardPerFrameM = 0.05;
constexpr double kRightPerFrameM = 0.002;
constexpr double kYawPerFrameRad = 0.003;

struct Point3
{
  // ROS optical frame 관례에 맞춰 x=right, y=down, z=forward 단위를 m로 사용한다.
  double x{0.0};
  double y{0.0};
  double z{0.0};
};

Point3 current_from_previous(const Point3 & previous)
{
  // p_prev = R(delta_yaw) * p_cur + t 이므로
  // p_cur = R(-delta_yaw) * (p_prev - t)다. 이는 두 frame 사이의 강체운동 ground truth다.
  const double shifted_x = previous.x - kRightPerFrameM;
  const double shifted_z = previous.z - kForwardPerFrameM;
  const double cosine = std::cos(kYawPerFrameRad);
  const double sine = std::sin(kYawPerFrameRad);
  return Point3{
    cosine * shifted_x + sine * shifted_z,
    previous.y,
    -sine * shifted_x + cosine * shifted_z};
}

double deterministic_subpixel_noise(const std::size_t index, const std::uint32_t sequence)
{
  // 실행할 때마다 같은 결과를 얻는 [-0.02, 0.02] px 잡음이다. RNG나 동적 상태가 없다.
  const auto code = static_cast<int>((index * 17U + static_cast<std::size_t>(sequence) * 11U) % 9U);
  return static_cast<double>(code - 4) * 0.005;
}
}  // namespace

class StereoCorrespondenceSimulator final : public rclcpp::Node
{
public:
  StereoCorrespondenceSimulator()
  : Node("stereo_correspondence_simulator")
  {
    // CameraInfo는 보정값 snapshot이다. Reliable + Transient Local을 쓰면 늦게 시작한 estimator도
    // 마지막 K/P 행렬을 즉시 받는다. 고속 sensor sample과 다른 QoS를 선택한 이유다.
    const auto calibration_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    left_info_publisher_ = create_publisher<sensor_msgs::msg::CameraInfo>(
      "/stereo/left/camera_info", calibration_qos);
    right_info_publisher_ = create_publisher<sensor_msgs::msg::CameraInfo>(
      "/stereo/right/camera_info", calibration_qos);

    // Correspondence는 매 frame 새 값으로 대체되는 sensor stream이므로 SensorDataQoS
    // (best effort, volatile, 작은 history)를 사용해 오래된 backlog보다 최신 관측을 우선한다.
    observation_publisher_ =
      create_publisher<daily_robotics_2026_10_26::msg::StereoCorrespondences>(
      "/stereo/correspondences", rclcpp::SensorDataQoS());

    // truth는 독립 auditor만 읽는 시뮬레이션 reference다. 실제 제품 graph에는 없어야 한다.
    truth_publisher_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      "/sim/stereo_truth", rclcpp::QoS(10).reliable());

    publish_calibration();

    // 30 Hz acquisition event마다 observation과 truth에 같은 stamp를 기록한다.
    timer_ = create_wall_timer(
      33333333ns, std::bind(&StereoCorrespondenceSimulator::publish_frame, this));

    RCLCPP_INFO(
      get_logger(),
      "30 Hz rectified stereo: %ux%u, fx=%.1f px, baseline=%.3f m, matches=%zu",
      kWidth, kHeight, kFxPx, kBaselineM, kMatchCount);
  }

private:
  // 좌/우 CameraInfo의 P 행렬로 rectified stereo baseline 계약을 만든다.
  void publish_calibration()
  {
    const auto stamp = now();
    sensor_msgs::msg::CameraInfo left;
    left.header.stamp = stamp;
    left.header.frame_id = "left_camera_optical_frame";
    left.height = kHeight;
    left.width = kWidth;
    left.distortion_model = "plumb_bob";
    left.d.assign(5U, 0.0);
    // K=[fx,0,cx; 0,fy,cy; 0,0,1]는 row-major camera intrinsic 행렬이다.
    left.k = {kFxPx, 0.0, kCxPx, 0.0, kFyPx, kCyPx, 0.0, 0.0, 1.0};
    left.r = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0};
    left.p = {
      kFxPx, 0.0, kCxPx, 0.0,
      0.0, kFyPx, kCyPx, 0.0,
      0.0, 0.0, 1.0, 0.0};

    auto right = left;
    right.header.frame_id = "right_camera_optical_frame";
    // Rectified right projection의 Tx=-fx*B다. estimator는 B=-P[3]/P[0]으로 복원한다.
    right.p[3] = -kFxPx * kBaselineM;

    left_info_publisher_->publish(left);
    right_info_publisher_->publish(right);
  }

  // 48개의 bounded stereo match와 동일 stamp의 누적 camera pose truth를 publish한다.
  void publish_frame()
  {
    const auto stamp = now();
    daily_robotics_2026_10_26::msg::StereoCorrespondences observation;
    observation.header.stamp = stamp;
    observation.header.frame_id = "left_camera_optical_frame";
    observation.sequence = sequence_;
    observation.count = static_cast<std::uint32_t>(kMatchCount);

    for (std::size_t i = 0U; i < kMatchCount; ++i) {
      // 넓은 image 영역과 여러 depth에 분산된 정적 landmark를 만든다.
      const auto column = static_cast<double>(i % 8U);
      const auto row = static_cast<double>(i / 8U);
      const Point3 previous{
        -1.75 + 0.50 * column,
        -0.85 + 0.34 * row,
        4.5 + 0.62 * row + 0.14 * static_cast<double>(i % 3U)};
      const Point3 current = current_from_previous(previous);

      // Rectified pinhole 식: u=cx+fx*X/Z, v=cy+fy*Y/Z, disparity=fx*B/Z.
      const double previous_u = kCxPx + kFxPx * previous.x / previous.z;
      const double previous_v = kCyPx + kFyPx * previous.y / previous.z;
      const double current_u = kCxPx + kFxPx * current.x / current.z;
      const double current_v = kCyPx + kFyPx * current.y / current.z;
      const double previous_disparity = kFxPx * kBaselineM / previous.z;
      const double current_disparity = kFxPx * kBaselineM / current.z;
      const double noise = deterministic_subpixel_noise(i, sequence_);

      observation.previous_u[i] = previous_u + 0.5 * noise;
      observation.previous_v[i] = previous_v - 0.25 * noise;
      observation.previous_disparity[i] = previous_disparity + noise;
      observation.current_u[i] = current_u - 0.5 * noise;
      observation.current_v[i] = current_v + 0.25 * noise;
      observation.current_disparity[i] = current_disparity - noise;

      // 약 1/7의 correspondence는 dynamic object/잘못된 descriptor match를 흉내 낸 gross outlier다.
      // disparity는 여전히 양수라 단순 유효범위 검사만으로는 걸러지지 않고 기하 consensus가 필요하다.
      if (((i + static_cast<std::size_t>(sequence_)) % 7U) == 0U) {
        observation.current_u[i] += (i % 2U == 0U) ? 28.0 : -24.0;
        observation.current_disparity[i] *= 0.58;
      }
    }

    // 누적 pose는 T_world_current = T_world_previous * T_previous_current 합성이다.
    const double cosine = std::cos(world_yaw_);
    const double sine = std::sin(world_yaw_);
    world_right_ += cosine * kRightPerFrameM - sine * kForwardPerFrameM;
    world_forward_ += sine * kRightPerFrameM + cosine * kForwardPerFrameM;
    world_yaw_ += kYawPerFrameRad;

    geometry_msgs::msg::PoseStamped truth;
    truth.header.stamp = stamp;
    truth.header.frame_id = "odom";
    // base 좌표의 +x forward, +y left로 내보내므로 optical right는 -y가 된다.
    truth.pose.position.x = world_forward_;
    truth.pose.position.y = -world_right_;
    truth.pose.orientation.z = std::sin(0.5 * world_yaw_);
    truth.pose.orientation.w = std::cos(0.5 * world_yaw_);

    // topic 간 DDS 도착 순서는 보장되지 않지만 stamp는 같으므로 auditor가 순서와 무관하게 join한다.
    truth_publisher_->publish(truth);
    observation_publisher_->publish(observation);
    ++sequence_;
  }

  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr left_info_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr right_info_publisher_;
  rclcpp::Publisher<daily_robotics_2026_10_26::msg::StereoCorrespondences>::SharedPtr
    observation_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr truth_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;

  std::uint32_t sequence_{0U};
  double world_right_{0.0};
  double world_forward_{0.0};
  double world_yaw_{0.0};
};

int main(int argc, char ** argv)
{
  // init은 ROS 인자와 rmw/DDS를 준비하고, spin은 timer callback event loop를 유지한다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<StereoCorrespondenceSimulator>());
  rclcpp::shutdown();
  return 0;
}
