#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace rv {

using Clock = std::chrono::system_clock;

struct Frame {
  std::uint64_t sequence{};
  Clock::time_point captured_at{};
  int width{};
  int height{};
  std::vector<std::uint8_t> pixels;  // MVP: CPU BGR. Jetson adapter uses an NVMM handle.
};

struct Detection {
  std::string label;
  float confidence{};
  float x{};
  float y{};
  float width{};
  float height{};
};

struct DetectionEvent {
  std::string device_id;
  std::uint64_t sequence{};
  Clock::time_point captured_at{};
  std::string model_version;
  std::vector<Detection> detections;
};

struct PipelineStats {
  std::uint64_t captured{};
  std::uint64_t inferred{};
  std::uint64_t published{};
  std::uint64_t dropped{};
};

}  // namespace rv

