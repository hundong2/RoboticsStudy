#pragma once

#include "rv/bounded_queue.hpp"
#include "rv/interfaces.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace rv {

struct PipelineOptions {
  std::string device_id{"jetson-dev-001"};
  std::size_t frame_queue_capacity{2};
  std::chrono::milliseconds heartbeat_interval{5000};
};

class Pipeline {
 public:
  Pipeline(PipelineOptions options, std::unique_ptr<ICamera> camera,
           std::unique_ptr<IDetector> detector, std::unique_ptr<IEventSink> sink);
  ~Pipeline();

  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  void Start();
  void Stop();
  [[nodiscard]] PipelineStats Stats() const;

 private:
  void CaptureLoop(std::stop_token stop);
  void InferenceLoop(std::stop_token stop);

  PipelineOptions options_;
  std::unique_ptr<ICamera> camera_;
  std::unique_ptr<IDetector> detector_;
  std::unique_ptr<IEventSink> sink_;
  LatestQueue<Frame> frames_;
  std::jthread capture_thread_;
  std::jthread inference_thread_;
  std::atomic_uint64_t captured_{};
  std::atomic_uint64_t inferred_{};
  std::atomic_uint64_t published_{};
};

}  // namespace rv

