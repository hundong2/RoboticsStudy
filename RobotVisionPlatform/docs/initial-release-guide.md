# 초기 릴리스 가이드 (v0.1.0)

v0.1.0의 목적은 **합성 장치 → 이벤트 수집 서버 → 웹 관제 화면**의 end-to-end 계약 검증입니다. 실제 카메라 영상/WebRTC/TensorRT는 다음 마일스톤입니다.

## 사전 조건

- 개발 PC: CMake 3.22+, C++20 compiler, .NET 10 SDK, Docker(선택)
- Jetson: JetPack/펌웨어 확인, 19V 정격 전원, 충분한 냉각, 유선 LAN 우선
- 배포 환경: 장치 ID, 서버 주소, 시간 동기화, 인증서 발급 계획

## 1. 검증 빌드

```bash
./scripts/build.sh
./scripts/test.sh
```

Windows PowerShell에서는 `./scripts/build.ps1`, `./scripts/test.ps1`을 사용합니다. C++ compiler가 없는 PC에서는 .NET만 검증되고 CI Linux job이 C++ 빌드를 보완합니다.

## 2. 서버 기동

```bash
docker compose -f deploy/compose/docker-compose.yml up --build
curl http://localhost:5080/health
```

또는 `dotnet run --project server/src/RobotVision.Server.Api`로 실행합니다.

## 3. 장치 smoke test

현재 기본 agent는 stdout sink를 사용합니다.

```bash
./device/build/robot_vision_device --device-id jetson-001
```

서버 계약은 아래 이벤트로 검증합니다.

```bash
curl -X POST http://localhost:5080/api/events/detections \
  -H 'content-type: application/json' \
  -d '{"deviceId":"jetson-001","sequence":1,"modelVersion":"demo/0.1.0","detections":[]}'
curl http://localhost:5080/api/devices/jetson-001
```

## 4. 릴리스 산출물

- `robot_vision_device` arm64 binary 또는 `.deb`
- server container image(digest로 고정)
- model bundle + manifest + signature
- `device.toml` schema/version과 migration note
- SBOM, checksum, release notes, JetPack/DeepStream 호환표

tag는 SemVer를 사용합니다. 장치 binary, 서버, 모델은 서로 다른 버전을 가지며 release manifest에서 검증된 조합을 선언합니다.

## 5. 단계 배포와 롤백

1. lab device 1대에 설치하고 health/FPS/온도/이벤트를 30분 확인
2. canary cohort에서 24시간 관찰
3. cohort를 점진 확대
4. crash loop, latency/temperature/drop guardrail 위반 시 이전 symlink와 systemd service로 즉시 rollback

현재 골격은 자동 OTA를 구현하지 않았습니다. 인증·서명·atomic switch가 완료되기 전 원격 덮어쓰기는 금지합니다.
