#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_09_20/msg/terrain_patch.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

// 제약 MCU의 주기 실행을 데스크톱 ROS 2에서 재현하는 교육용 소스 노드다.
// 실제 펌웨어라면 같은 순서를 rclc Executor의 trigger/LET 설정으로 옮겨야 한다.
class McuLetSource final : public rclcpp::Node {
 public:
  McuLetSource() : Node("mcu_let_source") {
    // SensorDataQoS는 최신 센서 샘플의 낮은 지연을 우선하는 best-effort 프로필이다.
    // depth=1로 덮어써서 느린 소비자가 과거 지형 패치를 쌓지 않게 한다.
    auto sensor_qos = rclcpp::SensorDataQoS();
    sensor_qos.keep_last(1);
    publisher_ = create_publisher<Patch>("/terrain_patch", sensor_qos);

    // wall timer는 50 ms마다 시작되는 MCU 주기를 모델링한다.
    // 콜백 내부의 sample→snapshot→publish 순서가 사용자 정의 순차 실행의 핵심이다.
    timer_ = create_wall_timer(50ms, [this]() { run_period(); });
  }

 private:
  using Patch = daily_robotics_2026_09_20::msg::TerrainPatch;
  static constexpr std::size_t kWidth = 8;
  static constexpr std::size_t kCellCount = kWidth * kWidth;
  static constexpr float kResolutionM = 0.10F;

  // rclcpp::Time을 메시지 타입으로 옮길 때 초/나노초를 직접 분리한다.
  // 이 방식은 배포판별 편의 함수 차이를 피하고 wire timestamp의 의미를 분명히 한다.
  static void set_stamp(const rclcpp::Time & now, builtin_interfaces::msg::Time & stamp) {
    const auto nanoseconds = now.nanoseconds();
    stamp.sec = static_cast<std::int32_t>(nanoseconds / 1000000000LL);
    stamp.nanosec = static_cast<std::uint32_t>(nanoseconds % 1000000000LL);
  }

  // 센서 취득 단계를 모사한다. 평면 경사 위에 국소 돌출을 더해 통과성 비용이 변하게 한다.
  // z(x,y)=0.02x+0.20 exp(-r²/1.2)는 전진 경사와 돌출 장애물을 함께 표현한다.
  void sample_height_sensor() {
    for (std::size_t row = 0; row < kWidth; ++row) {
      for (std::size_t col = 0; col < kWidth; ++col) {
        const auto index = row * kWidth + col;
        const float dx = static_cast<float>(col) - 5.0F;
        const float dy = static_cast<float>(row) - 4.0F;
        const float bump = 0.20F * std::exp(-(dx * dx + dy * dy) / 1.2F);
        sampled_heights_[index] = 0.02F * static_cast<float>(col) + bump;
      }
    }
  }

  // 한 주기의 시작에서 입력을 로컬 고정 배열에 모두 복사한 뒤 결과를 한 번만 발행한다.
  // 이는 LET의 입력 스냅샷 직관을 보여 주지만, rclc 자체나 출력 지연 커밋을 구현한 것은 아니다.
  void run_period() {
    sample_height_sensor();

    Patch message;
    set_stamp(now(), message.stamp);
    message.sequence = sequence_++;
    message.resolution_m = kResolutionM;
    message.width = static_cast<std::uint8_t>(kWidth);
    message.height = static_cast<std::uint8_t>(kWidth);
    message.elevation_m = sampled_heights_;
    message.valid.fill(1U);

    // publish는 고정 길이 메시지를 DDS로 넘긴다. 실제 micro-ROS에서는 rmw_microxrcedds를 거친다.
    publisher_->publish(message);
    if ((message.sequence % 20U) == 0U) {
      RCLCPP_INFO(get_logger(), "LET snapshot seq=%u cells=%zu", message.sequence, kCellCount);
    }
  }

  std::array<float, kCellCount> sampled_heights_{};
  std::uint32_t sequence_{0};
  rclcpp::Publisher<Patch>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  // spin은 wall timer의 준비 상태를 기다리고 주기 콜백을 실행한다.
  // 이 데스크톱 예제의 spin을 실제 MCU rclc_executor_spin_period와 혼동하면 안 된다.
  rclcpp::spin(std::make_shared<McuLetSource>());
  rclcpp::shutdown();
  return 0;
}
