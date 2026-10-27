#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "daily_robotics_2026_10_27/msg/event_packet.hpp"
#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr std::size_t kAnchorCount = 16U;
constexpr std::size_t kSamplesPerAnchor = 8U;
constexpr std::size_t kEventCount = kAnchorCount * kSamplesPerAnchor;
constexpr std::uint16_t kWidth = 64U;
constexpr std::uint16_t kHeight = 48U;
constexpr double kTruthVxPxS = 200.0;
constexpr double kTruthVyPxS = -200.0;
constexpr std::int32_t kFirstOffsetUs = -35000;
constexpr std::int32_t kOffsetStepUs = 5000;
}  // namespace

class EventPacketSimulator final : public rclcpp::Node
{
public:
  EventPacketSimulator()
  : Node("event_packet_simulator")
  {
    // 이벤트는 최신성이 중요한 sensor stream이다. SensorDataQoS는 작은 volatile history와
    // best-effort 전달을 사용해 느린 consumer 때문에 오래된 packet이 계속 쌓이지 않게 한다.
    event_publisher_ = create_publisher<daily_robotics_2026_10_27::msg::EventPacket>(
      "/event_camera/events", rclcpp::SensorDataQoS());

    // truth는 estimator가 보지 않고 auditor만 읽는 독립 reference다. 손실 없이 exact stamp로
    // 결합할 수 있도록 reliable QoS를 사용한다.
    truth_publisher_ = create_publisher<geometry_msgs::msg::Vector3Stamped>(
      "/sim/event_flow_truth", rclcpp::QoS(10).reliable());

    // create_wall_timer는 ROS simulation time과 무관한 steady wall clock으로 100 Hz callback을 만든다.
    // 실제 event driver라면 sensor interrupt/driver packet 도착이 이 callback 역할을 대신한다.
    timer_ = create_wall_timer(10ms, std::bind(&EventPacketSimulator::publish_packet, this));

    RCLCPP_INFO(
      get_logger(),
      "100 Hz synthetic event packets: %zu fixed events, truth=(%.1f, %.1f) px/s",
      kEventCount, kTruthVxPxS, kTruthVyPxS);
  }

private:
  // 16개 edge anchor를 8개 시각에서 관측해 128-event packet을 만든다.
  // 이 노드는 event camera driver의 timestamp/frame/polarity 계약을 학습시키는 역할이다.
  void publish_packet()
  {
    const auto stamp = now();
    daily_robotics_2026_10_27::msg::EventPacket packet;
    packet.header.stamp = stamp;
    packet.header.frame_id = "event_camera_optical_frame";
    packet.sequence = sequence_;
    packet.width = kWidth;
    packet.height = kHeight;
    packet.count = static_cast<std::uint16_t>(kEventCount);

    for (std::size_t anchor = 0U; anchor < kAnchorCount; ++anchor) {
      // anchor는 64x48 영상 전체에 분산한다. 한 점만 반복하면 velocity의 2D 관측성이 약해진다.
      const auto column = static_cast<double>(anchor % 4U);
      const auto row = static_cast<double>(anchor / 4U);
      const double reference_x = 8.0 + 16.0 * column;
      const double reference_y = 8.0 + 10.0 * row;

      for (std::size_t sample = 0U; sample < kSamplesPerAnchor; ++sample) {
        const std::size_t index = anchor * kSamplesPerAnchor + sample;
        const auto dt_us = kFirstOffsetUs +
          static_cast<std::int32_t>(sample) * kOffsetStepUs;
        const double dt_s = static_cast<double>(dt_us) * 1.0e-6;

        // 등속 optical flow 모델 x(t)=x_ref+v_x(t-t_ref)를 그대로 코드로 옮겼다.
        // header.stamp가 packet 기준 시각 t_ref이고 dt_us<=0이 각 event의 상대 측정시각이다.
        // 5 ms 간격에서는 오늘의 200/-200 px/s truth가 정확히 1 pixel씩 이동하므로,
        // 정수 sensor 좌표의 양자화가 x/y 축 중 하나를 편향시키지 않는다.
        const long measured_x = std::lround(reference_x + kTruthVxPxS * dt_s);
        const long measured_y = std::lround(reference_y + kTruthVyPxS * dt_s);
        packet.x[index] = static_cast<std::uint16_t>(
          std::clamp(measured_x, 0L, static_cast<long>(kWidth - 1U)));
        packet.y[index] = static_cast<std::uint16_t>(
          std::clamp(measured_y, 0L, static_cast<long>(kHeight - 1U)));
        packet.dt_us[index] = dt_us;
        // polarity=true/false는 log-intensity 증가/감소를 뜻한다. anchor마다 번갈아 배치해
        // estimator가 서로 반대인 edge를 한 histogram에서 상쇄하지 않게 한다.
        packet.polarity[index] = (anchor % 2U) == 0U;
      }
    }

    geometry_msgs::msg::Vector3Stamped truth;
    truth.header.stamp = stamp;
    truth.header.frame_id = packet.header.frame_id;
    truth.vector.x = kTruthVxPxS;
    truth.vector.y = kTruthVyPxS;
    truth.vector.z = 0.0;

    // topic 간 도착 순서는 DDS가 보장하지 않는다. auditor는 같은 acquisition stamp를 key로 join한다.
    truth_publisher_->publish(truth);
    event_publisher_->publish(packet);
    ++sequence_;
  }

  rclcpp::Publisher<daily_robotics_2026_10_27::msg::EventPacket>::SharedPtr event_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::Vector3Stamped>::SharedPtr truth_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::uint32_t sequence_{0U};
};

int main(int argc, char ** argv)
{
  // init은 ROS 인자와 DDS middleware를 준비하고, spin은 timer callback event loop를 유지한다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EventPacketSimulator>());
  rclcpp::shutdown();
  return 0;
}
