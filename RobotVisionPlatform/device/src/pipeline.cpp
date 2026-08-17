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
  // Start를 두 번 호출해 worker가 중복 생성되는 것을 방지합니다.
  if (capture_thread_.joinable() || inference_thread_.joinable()) return;
  // std::jthread는 callable에 stop_token을 자동으로 전달합니다.
  capture_thread_ = std::jthread([this](std::stop_token stop) { CaptureLoop(stop); });
  inference_thread_ = std::jthread([this](std::stop_token stop) { InferenceLoop(stop); });
}

void Pipeline::Stop() {
  // 먼저 종료 의사를 전달한 뒤 queue를 닫아 Pop에서 기다리는 consumer도 깨웁니다.
  capture_thread_.request_stop();
  inference_thread_.request_stop();
  frames_.Close();
  // join은 worker가 adapter를 더 이상 사용하지 않을 때까지 기다려 수명 문제를 막습니다.
  if (capture_thread_.joinable()) capture_thread_.join();
  if (inference_thread_.joinable()) inference_thread_.join();
}

PipelineStats Pipeline::Stats() const {
  // 서로 다른 worker가 counter를 수정하므로 atomic load로 data race를 피합니다.
  return {.captured = captured_.load(),
          .inferred = inferred_.load(),
          .published = published_.load(),
          .dropped = frames_.dropped()};
}

void Pipeline::CaptureLoop(std::stop_token stop) {
  try {
    while (!stop.stop_requested()) {
      // 실제 adapter는 여기서 V4L2/NvArgus/GStreamer 프레임을 기다립니다.
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
    // Pop은 새 프레임, queue close, stop 요청 중 하나가 발생할 때 깨어납니다.
    auto frame = frames_.Pop(stop);
    if (!frame) break;
    auto detections = detector_->Infer(*frame);
    ++inferred_;
    // model version과 원본 sequence를 함께 보내 서버가 결과의 출처를 추적하게 합니다.
    DetectionEvent event{.device_id = options_.device_id,
                         .sequence = frame->sequence,
                         .captured_at = frame->captured_at,
                         .model_version = detector_->Version(),
                         .detections = std::move(detections)};
    // false는 전송 실패를 의미합니다. production sink는 자체 retry/spool 정책을 가져야 합니다.
    if (sink_->Publish(event)) ++published_;
  }
}

}  // namespace rv
