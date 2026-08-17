# Jetson 최적화

1. PyTorch 모델을 ONNX로 export하고 ONNX Runtime 또는 Polygraphy로 출력 parity를 확인합니다.
2. TensorRT engine은 배포 대상과 동일한 JetPack/TensorRT 환경에서 생성합니다.
3. FP16 baseline을 먼저 만들고 정확도와 메모리가 필요할 때 INT8을 검토합니다.
4. GStreamer/DeepStream의 NVMM surface를 유지해 CPU 복사를 피합니다.
5. capture, preprocess, inference, encode의 latency를 각각 측정합니다.
6. queue depth를 제한하고 실시간 영상은 backlog보다 최신 프레임을 우선합니다.
7. 고정 max clock 결과만 보고 용량을 산정하지 말고 운영 power mode에서 soak test합니다.

DeepStream은 다중 스트림, tracker, OSD, inference plugin을 결합할 때 우선 검토하고, 단일 모델과 특수 후처리가 중심이면 직접 TensorRT adapter가 더 단순할 수 있습니다.

