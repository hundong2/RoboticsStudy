#include "rv/interfaces.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace rv {
namespace {

class SyntheticCamera final : public ICamera {
 public:
  SyntheticCamera(int width, int height, int fps)
      : width_(width), height_(height), interval_(1000 / (fps <= 0 ? 1 : fps)) {}

  Frame Read(std::stop_token stop) override {
    std::this_thread::sleep_for(interval_);
    if (stop.stop_requested()) return {};
    Frame frame{.sequence = ++sequence_,
                .captured_at = Clock::now(),
                .width = width_,
                .height = height_,
                .pixels = {}};
    // Synthetic mode avoids allocating full images; real adapters retain NVMM buffers.
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
    std::cout << "device=" << event.device_id << " sequence=" << event.sequence
              << " model=" << event.model_version
              << " detections=" << event.detections.size() << '\n';
    return true;
  }
};

}  // namespace

std::unique_ptr<ICamera> MakeSyntheticCamera(int width, int height, int fps) {
  return std::make_unique<SyntheticCamera>(width, height, fps);
}
std::unique_ptr<IDetector> MakeDemoDetector() { return std::make_unique<DemoDetector>(); }
std::unique_ptr<IEventSink> MakeConsoleEventSink() {
  return std::make_unique<ConsoleEventSink>();
}

}  // namespace rv
