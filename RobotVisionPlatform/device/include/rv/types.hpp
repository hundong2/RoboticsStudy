#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace rv {

/// 시스템 간 timestamp에 사용하는 실제 시각 clock입니다.
/// 성능 측정에는 시간이 역행할 수 없는 steady_clock을 별도로 사용해야 합니다.
using Clock = std::chrono::system_clock;

/// 카메라가 한 번 촬영한 영상 프레임과 메타데이터입니다.
struct Frame {
  /// 장치 프로세스 안에서 단조 증가하는 프레임 번호입니다.
  std::uint64_t sequence{};
  /// 카메라 adapter가 프레임 획득을 완료한 UTC 기준 시각입니다.
  Clock::time_point captured_at{};
  /// pixel buffer의 가로/세로 크기입니다.
  int width{};
  int height{};
  /// MVP의 CPU BGR byte 배열입니다. Jetson adapter에서는 복사 비용을 피하기 위해
  /// 이 필드 대신 NVMM/CUDA buffer handle을 소유하는 payload로 확장합니다.
  std::vector<std::uint8_t> pixels;  // MVP: CPU BGR. Jetson adapter uses an NVMM handle.
};

/// 정규화 좌표로 표현한 객체 하나의 탐지 결과입니다.
/// x/y는 좌상단이며 모든 좌표 값은 원본 크기와 무관하게 0.0~1.0을 사용합니다.
struct Detection {
  /// labels.txt와 같은 순서를 사용하는 사람이 읽을 수 있는 class 이름입니다.
  std::string label;
  /// 모델이 계산한 신뢰도입니다. 보통 0.0~1.0 범위입니다.
  float confidence{};
  float x{};
  float y{};
  float width{};
  float height{};
};

/// 한 프레임의 탐지 결과를 서버로 전송하기 위한 묶음입니다.
struct DetectionEvent {
  std::string device_id;
  std::uint64_t sequence{};
  Clock::time_point captured_at{};
  std::string model_version;
  std::vector<Detection> detections;
};

/// 파이프라인 상태를 외부에서 읽을 수 있는 누적 counter snapshot입니다.
struct PipelineStats {
  /// 카메라에서 성공적으로 받은 전체 프레임 수입니다.
  std::uint64_t captured{};
  /// detector에 전달한 전체 프레임 수입니다.
  std::uint64_t inferred{};
  /// sink가 성공으로 응답한 전체 이벤트 수입니다.
  std::uint64_t published{};
  /// queue가 가득 차 오래된 프레임을 제거한 횟수입니다.
  std::uint64_t dropped{};
};

}  // namespace rv
