#include <algorithm>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_09_20/msg/terrain_patch.hpp"
#include "daily_robotics_2026_09_20/msg/transport_stats.hpp"
#include "rclcpp/rclcpp.hpp"

// 고정 크기 TerrainPatch가 작은 XRCE 전송 버퍼를 몇 조각으로 통과하는지 계산한다.
// 패킷을 실제로 캡처하지 않으므로 수치는 명시된 가정 아래의 설계 예산이며 측정값이 아니다.
class TransportBudgetMonitor final : public rclcpp::Node {
 public:
  TransportBudgetMonitor() : Node("transport_budget_monitor") {
    auto sensor_qos = rclcpp::SensorDataQoS();
    sensor_qos.keep_last(1);
    subscription_ = create_subscription<Patch>(
        "/terrain_patch", sensor_qos,
        [this](Patch::ConstSharedPtr message) { on_patch(*message); });

    // 진단값은 유실보다 관찰 가능성이 중요하므로 reliable + transient_local 스냅샷으로 둔다.
    const auto diagnostic_qos = rclcpp::QoS(1).reliable().transient_local();
    publisher_ = create_publisher<Stats>("/transport_stats", diagnostic_qos);
  }

 private:
  using Patch = daily_robotics_2026_09_20::msg::TerrainPatch;
  using Stats = daily_robotics_2026_09_20::msg::TransportStats;

  // CDR 정렬을 포함한 이 메시지의 교육용 payload 추정치다.
  // Time(8)+sequence(4)+resolution(4)+width/height(2)+padding(2)+float[64](256)+uint8[64](64)=340 B.
  static constexpr std::uint16_t kSerializedPayloadBytes = 340U;
  static constexpr std::uint16_t kXrceMtuBytes = 128U;
  // 16 B는 XRCE 조각별 오버헤드 가정이다. 실제 값은 스트림/세션 설정과 구현을 캡처해 교체한다.
  static constexpr std::uint16_t kXrceOverheadBytes = 16U;
  // UDP(8 B)+IPv4(20 B) 헤더를 wire 예산에 별도 더한다. Ethernet 계층은 여기서 제외한다.
  static constexpr std::uint16_t kUdpIpv4OverheadBytes = 28U;
  static constexpr std::uint32_t kWireBudgetBytes = 520U;

  // ceil(payload/usable)을 정수 산술로 계산해 부동소수점 오차 없이 조각 수를 구한다.
  void on_patch(const Patch & patch) {
    const auto usable = static_cast<std::uint16_t>(kXrceMtuBytes - kXrceOverheadBytes);
    const auto fragments = static_cast<std::uint8_t>(
        (kSerializedPayloadBytes + usable - 1U) / usable);
    const auto wire_bytes = static_cast<std::uint32_t>(kSerializedPayloadBytes) +
        static_cast<std::uint32_t>(fragments) *
            static_cast<std::uint32_t>(kXrceOverheadBytes + kUdpIpv4OverheadBytes);

    Stats stats;
    stats.stamp = patch.stamp;
    stats.sequence = patch.sequence;
    stats.serialized_payload_bytes = kSerializedPayloadBytes;
    stats.xrce_mtu_bytes = kXrceMtuBytes;
    stats.usable_payload_bytes = usable;
    stats.fragment_count = fragments;
    stats.estimated_wire_bytes = wire_bytes;
    stats.budget_ok = wire_bytes <= kWireBudgetBytes;
    publisher_->publish(stats);
  }

  rclcpp::Subscription<Patch>::SharedPtr subscription_;
  rclcpp::Publisher<Stats>::SharedPtr publisher_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TransportBudgetMonitor>());
  rclcpp::shutdown();
  return 0;
}
