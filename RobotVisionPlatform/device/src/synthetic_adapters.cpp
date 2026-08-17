#include "rv/interfaces.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace rv {
namespace {

class SyntheticCamera final : public ICamera {
 public:
  /// width/height/fps만 흉내 내는 개발용 카메라입니다.
  SyntheticCamera(int width, int height, int fps)
      : width_(width), height_(height), interval_(1000 / (fps <= 0 ? 1 : fps)) {}

  Frame Read(std::stop_token stop) override {
    // 실제 카메라의 frame interval을 모사해 worker가 무한 속도로 돌지 않게 합니다.
    std::this_thread::sleep_for(interval_);
    if (stop.stop_requested()) return {};
    Frame frame{.sequence = ++sequence_,
                .captured_at = Clock::now(),
                .width = width_,
                .height = height_,
                .pixels = {}};
    // synthetic mode는 pixel을 사용하지 않으므로 큰 영상 배열 할당을 생략합니다.
    return frame;
  }

 private:
  int width_;
  int height_;
  std::chrono::milliseconds interval_;
  std::uint64_t sequence_{};
};

class DemoDetector final : public IDetector {
 public:
  std::vector<Detection> Infer(const Frame& frame) override {
    // 테스트에서 빈 결과와 탐지 결과를 모두 경험하도록 3의 배수 프레임만 탐지합니다.
    if (frame.sequence % 3 != 0) return {};
    const auto phase = static_cast<float>(frame.sequence % 100) / 100.0F;
    return {{.label = "demo-object",
             .confidence = 0.91F,
             .x = phase,
             .y = 0.2F,
             .width = 0.2F,
             .height = 0.3F}};
  }
  std::string Version() const override { return "demo-detector/0.1.0"; }
};

class ConsoleEventSink final : public IEventSink {
 public:
  bool Publish(const DetectionEvent& event) override {
    // stdout 출력이 성공했다고 간주하므로 pipeline smoke test에서 network가 필요 없습니다.
    std::cout << "device=" << event.device_id << " sequence=" << event.sequence
              << " model=" << event.model_version
              << " detections=" << event.detections.size() << '\n';
    return true;
  }
};

}  // namespace

std::unique_ptr<ICamera> MakeSyntheticCamera(int width, int height, int fps) {
  // factory 함수는 concrete type을 감춰 main이 interface에만 의존하게 합니다.
  return std::make_unique<SyntheticCamera>(width, height, fps);
}
std::unique_ptr<IDetector> MakeDemoDetector() { return std::make_unique<DemoDetector>(); }
std::unique_ptr<IEventSink> MakeConsoleEventSink() {
  return std::make_unique<ConsoleEventSink>();
}

}  // namespace rv
