#include "rv/pipeline.hpp"

#include <exception>
#include <iostream>
#include <utility>

namespace rv {

Pipeline::Pipeline(PipelineOptions options, std::unique_ptr<ICamera> camera,
                   std::unique_ptr<IDetector> detector, std::unique_ptr<IEventSink> sink)
    : options_(std::move(options)),
      camera_(std::move(camera)),
      detector_(std::move(detector)),
      sink_(std::move(sink)),
      frames_(options_.frame_queue_capacity) {}

Pipeline::~Pipeline() { Stop(); }

void Pipeline::Start() {
  if (capture_thread_.joinable() || inference_thread_.joinable()) return;
  capture_thread_ = std::jthread([this](std::stop_token stop) { CaptureLoop(stop); });
  inference_thread_ = std::jthread([this](std::stop_token stop) { InferenceLoop(stop); });
}

void Pipeline::Stop() {
  capture_thread_.request_stop();
  inference_thread_.request_stop();
  frames_.Close();
  if (capture_thread_.joinable()) capture_thread_.join();
  if (inference_thread_.joinable()) inference_thread_.join();
}

PipelineStats Pipeline::Stats() const {
  return {.captured = captured_.load(),
          .inferred = inferred_.load(),
          .published = published_.load(),
          .dropped = frames_.dropped()};
}

void Pipeline::CaptureLoop(std::stop_token stop) {
  try {
    while (!stop.stop_requested()) {
      auto frame = camera_->Read(stop);
      if (stop.stop_requested()) break;
      ++captured_;
      if (!frames_.Push(std::move(frame))) break;
    }
  } catch (const std::exception& error) {
    std::cerr << "capture_error=\"" << error.what() << "\"\n";
  }
  frames_.Close();
}

void Pipeline::InferenceLoop(std::stop_token stop) {
  while (!stop.stop_requested()) {
    auto frame = frames_.Pop(stop);
    if (!frame) break;
    auto detections = detector_->Infer(*frame);
    ++inferred_;
    DetectionEvent event{.device_id = options_.device_id,
                         .sequence = frame->sequence,
                         .captured_at = frame->captured_at,
                         .model_version = detector_->Version(),
                         .detections = std::move(detections)};
    if (sink_->Publish(event)) ++published_;
  }
}

}  // namespace rv

