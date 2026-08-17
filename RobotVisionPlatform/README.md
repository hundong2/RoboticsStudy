# Robot Vision Platform

Jetson Orin Nano Super에서 비전 추론을 수행하고, 서버에서 장치·영상·이벤트를 관제하며, 모델을 학습·배포·롤백하는 단일 저장소(monorepo)입니다.

> 현재 단계는 **MVP 기반 골격**입니다. 합성 카메라/탐지기로 전체 파이프라인과 서버 수집을 먼저 검증하고, 실제 CSI/USB 카메라·TensorRT·WebRTC 구현을 어댑터로 교체합니다.

## 처음 보는 분은 여기부터

로봇 비전이나 Jetson이 처음이라면 전체 문서를 한 번에 이해할 필요가 없습니다.

1. [초보자 시작 가이드](docs/getting-started-for-beginners.md)를 따라 서버를 실행합니다.
2. 브라우저에서 장치가 표시되는 것을 확인합니다.
3. [용어집](docs/glossary.md)에서 낯선 단어만 찾아봅니다.
4. 이후 관심 분야에 따라 `tips`, `device`, `server` 문서로 이동합니다.

### 지금 바로 할 수 있는 것

- .NET 서버 실행과 health check
- 예제 탐지 이벤트 전송
- 웹 화면에서 장치 ID, 모델 버전, 탐지 결과 확인
- 합성 카메라를 사용하는 C++20 파이프라인 빌드와 테스트

### 아직 구현되지 않은 것

- Jetson CSI/USB 카메라의 실제 프레임 입력
- TensorRT 모델의 실제 객체 탐지
- WebRTC 실시간 영상 재생
- gRPC/mTLS 장치 연결과 자동 모델 업데이트
- VLM/VLA 분석과 로봇 동작 명령

이 기능들은 오류가 아니라 아래 TODO에 따라 구현할 다음 단계입니다.

## 목표 아키텍처

```text
Camera -> bounded queue -> TensorRT/DeepStream -> overlay/encoder -> WebRTC (video)
                              |                         |
                              +-> gRPC events/status --+--> ASP.NET Core -> SignalR -> MAUI/Web UI
                                      ^                       |
                                      +------ commands --------+

Training -> ONNX -> TensorRT engine build on target -> signed model registry -> staged OTA/rollback
```

영상과 제어 경로를 분리합니다. H.264/H.265 하드웨어 인코딩 영상은 WebRTC(저지연) 또는 RTSP/SRT(불안정한 WAN)로 보내고, 작은 구조화 데이터와 명령은 protobuf/gRPC로 보냅니다. 서버는 ASP.NET Core, 관제 앱은 UI 공유가 쉬운 .NET MAUI Blazor Hybrid를 기준으로 합니다.

## 저장소 구조

```text
RobotVisionPlatform/
├── docs/                # 통합 설계·운영·초기 릴리스 가이드
├── tips/                # 학습 기초와 실전 가이드
├── device/              # Jetson에서 실행되는 C++20 프로젝트
├── server/              # ASP.NET Core 수집 서버와 MAUI 관제 앱
├── shared/proto/        # 장치/서버 공용 protobuf 계약
├── deploy/              # Compose, systemd, 배포 설정
├── scripts/             # 개발·검증 스크립트
└── ../.github/workflows/ # GitHub가 인식하는 저장소 루트 CI/릴리스 자동화
```

## 단계별 TODO

### Phase 0 — 기반과 계약

- [x] 모노레포 폴더와 문서 체계 생성
- [x] 장치 등록·상태·탐지 이벤트용 protobuf 초안 정의
- [x] C++20 파이프라인 코어와 서버 수집 API 골격 생성
- [x] 기본 CI, Docker Compose, systemd 배포 파일 생성
- [ ] 장치 PKI(mTLS), 인증서 발급/회전 정책 확정
- [ ] 카메라별 해상도·FPS·지연·보존 기간 SLO 확정

### Phase 1 — 단일 장치 MVP

- [ ] JetPack/DeepStream 호환표를 실제 장치 이미지에 고정
- [ ] CSI 또는 USB 카메라 GStreamer source 어댑터 구현
- [ ] 기준 모델을 ONNX로 내보내고 장치에서 TensorRT engine 생성
- [ ] 실제 탐지 메타데이터를 gRPC `Connect` 스트림으로 전송
- [ ] NVENC H.264 + WebRTC 송출 및 서버 signaling 구현
- [ ] 오프라인 store-and-forward와 재접속 검증

### Phase 2 — 관제와 운영

- [ ] MAUI 대시보드에 WebRTC 플레이어와 탐지 오버레이 연결
- [ ] PostgreSQL/TimescaleDB 이벤트 저장소와 S3 호환 클립 저장소 연결
- [ ] OpenTelemetry metrics/logs/traces 및 장치 health alert 추가
- [ ] 원격 설정, 재시작, 로그 묶음 수집 명령 추가
- [ ] 네트워크 손실·재부팅·전원 차단 chaos test 자동화

### Phase 3 — 모델 수명주기

- [ ] 데이터셋 버전 관리(DVC 또는 lakeFS)와 라벨 검수 흐름 확정
- [ ] 학습/평가 파이프라인과 모델 승인 게이트 구축
- [ ] 모델 manifest, SHA-256, 서명, 호환성 검증 추가
- [ ] canary → cohort → fleet 단계 배포와 자동 롤백 구현
- [ ] drift/오탐/미탐 샘플링 및 active-learning 루프 구현

### Phase 4 — VLM/VLA 확장

- [ ] 이벤트 기반 keyframe/clip 추출(상시 VLM 실행 금지)
- [ ] 서버 GPU의 VLM batch 분석과 구조화 결과 스키마 정의
- [ ] VLA 명령은 정책 엔진·허용 목록·human-in-the-loop를 통과하도록 구현
- [ ] 안전 정지 및 명령 감사 로그 검증

## 빠른 시작

필수 도구는 CMake 3.22+, C++20 컴파일러, .NET 10 SDK, Docker입니다.

```powershell
./scripts/build.ps1
./scripts/test.ps1
dotnet run --project server/src/RobotVision.Server.Api
```

명령은 `RobotVisionPlatform` 폴더에서 실행합니다. 자세한 준비물과 예상 결과는 [초보자 시작 가이드](docs/getting-started-for-beginners.md)에 있습니다.

서버 실행 후 `http://localhost:5080`, health check는 `/health`, 장치 목록은 `/api/devices`입니다. 장치 데모는 별도 터미널에서 실행합니다. Linux/Ninja는 `device/build/robot_vision_device`, Visual Studio generator는 보통 `device/build/Release/robot_vision_device.exe`에 생성됩니다. Boost HTTP adapter를 빌드했다면 `--server-host 127.0.0.1 --server-port 5080`을 더해 MVP end-to-end 전송을 확인할 수 있습니다.

## 설계 문서

- [시스템 아키텍처](docs/architecture.md)
- [초기 릴리스 가이드](docs/initial-release-guide.md)
- [Jetson 준비 및 배포](docs/jetson-deployment.md)
- [통신과 영상 전송](docs/streaming-and-protocols.md)
- [모델 수명주기](docs/model-lifecycle.md)
- [보안 및 운영](docs/security-and-operations.md)
- [학습 팁 인덱스](tips/README.md)
- [초보자 시작 가이드](docs/getting-started-for-beginners.md)
- [용어집](docs/glossary.md)
- [문제 해결](docs/troubleshooting.md)
- [ONNX/TensorRT 모델 배포 가이드](device/guides/README.md)

## 기술 기준

- Edge: C++20, CMake, `std::jthread`/`std::stop_token`, 선택형 Boost, GStreamer/DeepStream/TensorRT
- Control plane: protobuf + gRPC bidirectional streaming, TLS/mTLS
- Media plane: WebRTC + H.264 우선, RTSP/SRT 대안, keyframe JPEG는 디버그용
- Server: .NET 10, ASP.NET Core, SignalR, MAUI Blazor Hybrid
- Observability: OpenTelemetry, Prometheus 호환 metrics, 구조화 로그

버전 숫자는 예시보다 실제 JetPack–DeepStream 호환표를 우선합니다. 자세한 결정 근거와 공식 링크는 각 문서에 있습니다.
