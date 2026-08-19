# Robot Vision Reference

프로젝트에서 보이는 함수·타입·명령을 이름으로 찾아보는 사전입니다. 처음부터 끝까지 읽기보다 편집기 검색(`Ctrl+F`)으로 필요한 항목을 찾으세요.

## 언어별 사전

| 문서 | 찾을 수 있는 내용 |
|---|---|
| [C++20 장치 코드](cpp20-device-reference.md) | interface, smart pointer, move, jthread, stop_token, queue, atomic |
| [C# 서버 코드](csharp-server-reference.md) | record, DI, ConcurrentDictionary, Channel, BackgroundService, SignalR |
| [Python 모델 도구](python-model-tools-reference.md) | argparse, Path, hashing, ONNX, ONNX Runtime, NumPy |
| [스크립트·프로토콜](scripts-and-protocol-reference.md) | PowerShell, Bash, CMake, protobuf, gRPC 기본 사용법 |

## 기능에서 역으로 찾기

| 하고 싶은 일 | 찾아볼 항목 | 실제 시작 파일 |
|---|---|---|
| 카메라 종류 추가 | `ICamera`, `override`, factory | [`interfaces.hpp`](../device/include/rv/interfaces.hpp) |
| TensorRT detector 추가 | `IDetector`, `unique_ptr` | [`interfaces.hpp`](../device/include/rv/interfaces.hpp) |
| 프레임 지연 제어 | `LatestQueue`, `condition_variable_any` | [`bounded_queue.hpp`](../device/include/rv/bounded_queue.hpp) |
| 장치 event 수신 | Minimal API `MapPost` | [`Program.cs`](../server/src/RobotVision.Server.Api/Program.cs) |
| 최신 장치 상태 저장 | `ConcurrentDictionary`, `AddOrUpdate` | [`DeviceRegistry.cs`](../server/src/RobotVision.Server.Api/DeviceRegistry.cs) |
| 화면에 실시간 알림 | `Channel`, `BackgroundService`, SignalR | [`DetectionFanoutWorker.cs`](../server/src/RobotVision.Server.Api/DetectionFanoutWorker.cs) |
| ONNX 입출력 확인 | `onnx.load`, `checker`, shape inference | [`inspect_onnx.py`](../device/tools/model/inspect_onnx.py) |
| ONNX 추론 | `InferenceSession`, `session.run` | [`validate_onnx.py`](../device/tools/model/validate_onnx.py) |
| TensorRT engine 생성 | `trtexec`, dynamic shape | [`build_tensorrt_engine.sh`](../device/tools/model/build_tensorrt_engine.sh) |
| 통신 필드 추가 | protobuf `message`, `oneof` | [`device.proto`](../shared/proto/vision/v1/device.proto) |

## 표기 규칙

- `입력`: 함수가 받는 값
- `반환`: 함수가 호출자에게 돌려주는 값
- `수명`: 객체가 언제까지 유효한지
- `thread-safe`: 여러 thread에서 동시에 사용 가능한지
- `주의`: 이 프로젝트에서 자주 발생할 수 있는 실수

코드 동작의 전체 흐름은 [시스템 아키텍처](../docs/architecture.md), 모델 배포 흐름은 [Device 모델 배포 가이드](../device/guides/README.md)를 함께 참고합니다.
