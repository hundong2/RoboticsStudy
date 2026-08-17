#pragma once

#include "rv/types.hpp"

#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace rv {

/// 카메라 종류와 무관하게 파이프라인이 프레임을 읽는 계약입니다.
class ICamera {
 public:
  virtual ~ICamera() = default;

  /// 다음 프레임이 준비될 때까지 기다렸다 반환합니다.
  /// @param stop 종료 요청을 전달하는 C++20 stop token입니다.
  /// @return 캡처된 한 프레임입니다.
  /// @throws std::exception 카메라 연결이나 decode가 실패한 경우입니다.
  virtual Frame Read(std::stop_token stop) = 0;
};

/// TensorRT, DeepStream, test detector가 공통으로 구현할 추론 계약입니다.
class IDetector {
 public:
  virtual ~IDetector() = default;

  /// 프레임 하나를 동기적으로 추론합니다. 호출이 끝날 때까지 frame은 유효합니다.
  virtual std::vector<Detection> Infer(const Frame& frame) = 0;

  /// event와 metric에 기록할 모델 식별자/버전을 반환합니다.
  [[nodiscard]] virtual std::string Version() const = 0;
};

/// 탐지 이벤트의 최종 목적지를 추상화한 계약입니다.
class IEventSink {
 public:
  virtual ~IEventSink() = default;

  /// 이벤트를 전송합니다.
  /// @return 목적지가 이벤트를 받았으면 true, 재시도가 필요하면 false입니다.
  virtual bool Publish(const DetectionEvent& event) = 0;
};

/// 실제 카메라 없이 pipeline을 시험하는 일정 FPS의 synthetic camera를 만듭니다.
std::unique_ptr<ICamera> MakeSyntheticCamera(int width, int height, int fps);
/// 세 프레임마다 예제 객체를 반환하는 test detector를 만듭니다.
std::unique_ptr<IDetector> MakeDemoDetector();
/// 탐지 개수와 모델 정보를 stdout에 쓰는 sink를 만듭니다.
std::unique_ptr<IEventSink> MakeConsoleEventSink();

#ifdef RV_HAS_BOOST_HTTP
/// Boost.Beast로 JSON detection event를 HTTP POST하는 MVP sink를 만듭니다.
/// production에서는 연결 재사용, TLS, disk spool이 있는 gRPC adapter로 교체합니다.
std::unique_ptr<IEventSink> MakeHttpEventSink(std::string host, std::string port,
                                              std::string target);
#endif

}  // namespace rv
