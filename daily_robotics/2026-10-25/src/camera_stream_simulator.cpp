#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/image.hpp"

using namespace std::chrono_literals;

namespace
{
// 학습용 image 크기를 작게 고정해 downstream feature kernel의 최악 작업량을 계산 가능하게 한다.
constexpr std::uint32_t kWidth = 96U;
constexpr std::uint32_t kHeight = 72U;
constexpr double kFocalPx = 80.0;
constexpr double kPrincipalX = 47.5;
constexpr double kPrincipalY = 35.5;
constexpr double kSceneDepthM = 4.0;
constexpr double kTranslationPerFrameM = 0.05;

// 정수 world pixel 좌표에서 항상 같은 밝기를 반환하는 결정론적 texture다.
// 실제 카메라 driver 대신 무한히 이어지는 textured plane을 만드는 역할이며, heap 할당이나 RNG 상태가 없다.
std::uint8_t texture_at(const std::int32_t x, const std::int32_t y)
{
  // signed 좌표를 unsigned bit pattern으로 바꾼 뒤 avalanche hash를 적용한다.
  std::uint32_t value = static_cast<std::uint32_t>(x) * 0x45d9f3bU;
  value ^= static_cast<std::uint32_t>(y) * 0x119de1f3U;
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  return static_cast<std::uint8_t>(32U + (value % 192U));
}
}  // namespace

class CameraStreamSimulator final : public rclcpp::Node
{
public:
  CameraStreamSimulator()
  : Node("camera_stream_simulator")
  {
    // SensorDataQoS는 카메라처럼 최신 표본이 중요한 스트림에 맞는 best-effort/작은 history 프로필이다.
    image_publisher_ = create_publisher<sensor_msgs::msg::Image>(
      "/camera/image_rect", rclcpp::SensorDataQoS());

    // CameraInfo는 calibration snapshot이다. Reliable + Transient Local이면 늦게 시작한 tracker도
    // 마지막 intrinsic을 받을 수 있다. volatile image와 durability를 의도적으로 다르게 둔다.
    const auto info_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    info_publisher_ = create_publisher<sensor_msgs::msg::CameraInfo>(
      "/camera/camera_info", info_qos);

    // truth는 검증 노드만 읽는 reference이며 실제 제품 graph에는 존재하지 않는 신호다.
    truth_publisher_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      "/sim/camera_truth", rclcpp::QoS(10).reliable());

    // wall timer는 30 Hz camera acquisition을 흉내 낸다. header.stamp에는 callback 도착시간이 아니라
    // 이 acquisition event에서 한 번 읽은 동일한 시각을 image/info/truth 모두에 기록한다.
    timer_ = create_wall_timer(33333333ns, std::bind(&CameraStreamSimulator::publish_frame, this));

    RCLCPP_INFO(
      get_logger(),
      "30 Hz mono8 camera: %ux%u, fx=%.1f px, planar depth=%.1f m",
      kWidth, kHeight, kFocalPx, kSceneDepthM);
  }

private:
  // textured plane을 렌더링해 ROS Image/CameraInfo 계약과 metric truth를 동시에 제공한다.
  void publish_frame()
  {
    // rclcpp::Clock가 반환한 한 stamp를 세 topic에 공유해야 exact-stamp audit가 가능하다.
    const auto stamp = now();

    sensor_msgs::msg::Image image;
    image.header.stamp = stamp;
    // optical frame의 ROS 관례는 +x right, +y down, +z forward다.
    image.header.frame_id = "camera_optical_frame";
    image.height = kHeight;
    image.width = kWidth;
    // mono8은 pixel 하나가 unsigned 8-bit grayscale 한 채널임을 뜻한다.
    image.encoding = "mono8";
    image.is_bigendian = 0U;
    image.step = kWidth;
    // sensor_msgs/Image::data는 가변 배열이므로 simulator(non-RT side)에서만 resize한다.
    image.data.resize(static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight));

    // pinhole 식 u = fx * (X - x_camera) / Z에서 camera가 +x로 0.05 m 이동하면
    // 관측 feature는 du = -fx*dx/Z = -1 pixel/frame만큼 왼쪽으로 움직인다.
    const auto static_shift = static_cast<std::int32_t>(sequence_);
    for (std::uint32_t v = 0U; v < kHeight; ++v) {
      for (std::uint32_t u = 0U; u < kWidth; ++u) {
        const bool dynamic_patch = (u >= 36U && u < 58U && v >= 24U && v < 48U);
        // 중앙 patch만 배경과 다른 3 px/frame apparent motion을 주어 outlier를 만든다.
        const auto shift = dynamic_patch ? (3 * static_shift) : static_shift;
        const auto world_u = static_cast<std::int32_t>(u) + shift;
        const auto world_v = static_cast<std::int32_t>(v) + (dynamic_patch ? 211 : 0);
        const auto index = static_cast<std::size_t>(v) * static_cast<std::size_t>(kWidth) +
          static_cast<std::size_t>(u);
        image.data[index] = texture_at(world_u, world_v);
      }
    }

    sensor_msgs::msg::CameraInfo info;
    info.header = image.header;
    info.height = kHeight;
    info.width = kWidth;
    info.distortion_model = "plumb_bob";
    info.d.assign(5U, 0.0);
    // CameraInfo::k는 [fx,0,cx; 0,fy,cy; 0,0,1] 순서의 row-major intrinsic이다.
    info.k = {kFocalPx, 0.0, kPrincipalX, 0.0, kFocalPx, kPrincipalY, 0.0, 0.0, 1.0};
    // 이미 rectified image이므로 rectification matrix R은 identity다.
    info.r = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0};
    // monocular processed image의 P 왼쪽 3x3은 K와 같고 Tx=Ty=0이다.
    info.p = {
      kFocalPx, 0.0, kPrincipalX, 0.0,
      0.0, kFocalPx, kPrincipalY, 0.0,
      0.0, 0.0, 1.0, 0.0};

    geometry_msgs::msg::PoseStamped truth;
    truth.header.stamp = stamp;
    truth.header.frame_id = "map";
    truth.pose.position.x = static_cast<double>(sequence_) * kTranslationPerFrameM;
    truth.pose.orientation.w = 1.0;

    // 세 publish 호출은 같은 stamp를 가진다. DDS topic 간 도착 순서는 보장되지 않으므로 auditor가
    // 수신 순서를 가정하지 않고 stamp로 join해야 한다.
    info_publisher_->publish(info);
    truth_publisher_->publish(truth);
    image_publisher_->publish(image);
    ++sequence_;
  }

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr truth_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::uint32_t sequence_{0U};
};

int main(int argc, char ** argv)
{
  // init은 DDS/rmw와 ROS 인자 처리를 시작하고, shutdown은 middleware 자원을 정리한다.
  rclcpp::init(argc, argv);
  // spin은 timer callback이 실행될 수 있도록 executor event loop를 유지한다.
  rclcpp::spin(std::make_shared<CameraStreamSimulator>());
  rclcpp::shutdown();
  return 0;
}
