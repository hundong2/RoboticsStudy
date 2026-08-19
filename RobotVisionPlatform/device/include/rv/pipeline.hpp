#pragma once

#include "rv/bounded_queue.hpp"
#include "rv/interfaces.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace rv {

/// Pipeline의 장치 식별자와 실시간 처리 정책입니다.
struct PipelineOptions {
  std::string device_id{"jetson-dev-001"};
  /// 작게 유지할수록 처리량보다 최신 프레임과 낮은 지연을 우선합니다.
  std::size_t frame_queue_capacity{2};
  /// 향후 health heartbeat worker가 사용할 전송 주기입니다.
  std::chrono::milliseconds heartbeat_interval{5000};
};

/// camera, detector, event sink의 수명주기와 worker thread를 관리합니다.
///
/// 사용 예:
/// @code
/// Pipeline pipeline(options, MakeSyntheticCamera(640, 480, 30),
///                   MakeDemoDetector(), MakeConsoleEventSink());
/// pipeline.Start();
/// // ... application work ...
/// pipeline.Stop();
/// @endcode
class Pipeline {
 public:
  /// adapter 소유권을 Pipeline로 이전합니다. null adapter를 전달하면 안 됩니다.
  Pipeline(PipelineOptions options, std::unique_ptr<ICamera> camera,
           std::unique_ptr<IDetector> detector, std::unique_ptr<IEventSink> sink);
  /// 실행 중이면 먼저 Stop()하여 worker가 adapter보다 먼저 종료되게 합니다.
  ~Pipeline();

  // Worker와 adapter는 단일 소유이므로 Pipeline 복사를 금지합니다.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  /// capture/inference worker를 시작합니다. 이미 시작됐으면 아무 작업도 하지 않습니다.
  void Start();
  /// 두 worker에 종료를 요청하고 join이 끝날 때까지 기다립니다.
  void Stop();
  /// lock-free atomic counter와 queue drop 수를 한 시점의 값으로 반환합니다.
  [[nodiscard]] PipelineStats Stats() const;

 private:
  /// 카메라를 읽어 latest-frame queue에 넣는 producer loop입니다.
  void CaptureLoop(std::stop_token stop);
  /// queue에서 프레임을 꺼내 추론하고 event sink에 보내는 consumer loop입니다.
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
