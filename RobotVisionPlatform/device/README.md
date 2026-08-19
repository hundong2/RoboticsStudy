# Device Agent

C++20 기반 edge 파이프라인입니다. 기본 빌드는 외부 SDK 없이 합성 입력으로 실행됩니다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/robot_vision_device --device-id jetson-dev-001
```

Boost.System을 포함해 빌드했다면 실행 중인 MVP 서버로 바로 전송할 수 있습니다.

```bash
./build/robot_vision_device --device-id jetson-dev-001 --server-host 127.0.0.1 --server-port 5080
```

실제 장치 구현 순서는 `ICamera`의 GStreamer/NvArgus adapter, `IDetector`의 TensorRT/DeepStream adapter, `IEventSink`의 gRPC/mTLS adapter, `IVideoPublisher`의 WebRTC adapter입니다. 코어가 CPU `pixels`를 정의하지만 Jetson adapter에서는 NVMM/CUDA handle을 별도 frame payload로 확장해 zero-copy를 유지합니다.

Boost.System이 발견되면 Boost.Asio/Beast HTTP sink가 함께 컴파일됩니다. 이 sink는 서버 계약 smoke test용이며 장기 운영 경로는 공용 proto의 bidi gRPC입니다.

## 학습 모델을 장치에 넣기

모델 배포는 `PyTorch → ONNX → ONNX Runtime 검증 → Jetson TensorRT engine → bundle 배포` 순서로 진행합니다. 처음에는 [Device 모델 배포 가이드](guides/README.md)를 읽으세요.

- ONNX 검사·실행 도구: `tools/model/`
- TensorRT 변환 스크립트: `tools/model/build_tensorrt_engine.sh`
- DeepStream 설정 예제: `config/deepstream/config_infer_primary.example.txt`
- 모델 bundle 예제: `models/example/`

현재 `DemoDetector`는 합성 결과를 만드는 테스트 구현입니다. 실제 TensorRT/DeepStream detector를 `IDetector`에 연결하는 작업은 ONNX와 전·후처리 parity가 확인된 뒤 진행합니다.
