# 시스템 아키텍처

## 원칙

1. 장치는 네트워크가 끊겨도 추론과 안전 관련 작업을 계속한다.
2. 영상(media plane)과 이벤트/제어(control plane)를 분리해 장애와 대역폭을 격리한다.
3. 모든 큐는 bounded queue로 만들고, 실시간 경로에서는 오래된 프레임을 버려 지연 누적을 막는다.
4. 카메라·추론기·전송기는 인터페이스 뒤에 두어 USB/CSI, TensorRT/DeepStream, WebRTC/SRT를 교체한다.
5. 서버가 보내는 동작 명령은 정책 검사, 만료 시간, idempotency key, 감사 로그를 갖는다.

## 구성 요소

- `device`: 프레임 획득, 전처리, 추론, 로컬 규칙, 인코딩, 이벤트/상태 전송, 오프라인 버퍼
- `server/Api`: 장치 세션, ingest, 최신 상태, SignalR fan-out, 향후 저장소/VLM 작업 큐
- `server/Dashboard`: 웹/Windows/Android 관제 UI의 공유 Razor 컴포넌트
- `shared/proto`: wire contract의 단일 원본
- Object storage: 이벤트 클립/스냅샷. 원본 상시 업로드는 기본값이 아님
- PostgreSQL: 장치·모델·배포·이벤트 메타데이터

## 장치 파이프라인

```text
Capture (NVMM zero-copy)
  -> latest-frame queue
  -> batching/preprocess
  -> TensorRT inference
  -> local rule engine
  +-> protobuf event queue -> retrying gRPC transport
  +-> OSD -> NVENC -> WebRTC/SRT
```

코어는 합성 구현으로 테스트할 수 있습니다. 실제 Jetson adapter는 DeepStream 9.x의 C/C++ 플러그인 또는 GStreamer 앱 파이프라인으로 연결합니다. CUDA/NVMM 버퍼를 CPU로 복사하지 않는 것이 핵심입니다.

## 서버 처리

수집 handler는 무거운 VLM을 직접 실행하지 않습니다. 이벤트를 검증·저장하고 bounded channel/메시지 브로커에 넣은 뒤 즉시 응답합니다. 별도 worker가 keyframe/짧은 clip만 VLM에 batch로 전달합니다. 실시간 객체 추적은 edge에서, 의미 해석은 server에서 수행하는 계층형 추론입니다.

## 확장 지점

- `ICamera`: Synthetic → V4L2/GStreamer/NvArgus
- `IDetector`: Noop → ONNX Runtime → TensorRT/DeepStream
- `IEventSink`: stdout → gRPC/mTLS → broker gateway
- `IVideoPublisher`: disabled → WebRTC → SRT/RTSP
- `IAction`: log only → GPIO/ROS 2/PLC adapter

