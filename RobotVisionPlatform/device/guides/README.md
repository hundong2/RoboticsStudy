# Device 모델 배포 가이드

학습한 모델을 Jetson 장치에 넣을 때는 다음 순서로 진행합니다.

```text
PyTorch/학습 도구
  -> ONNX export
  -> ONNX 구조·연산 검증
  -> PC의 ONNX Runtime 기준 결과 저장
  -> Jetson에서 TensorRT engine 생성
  -> 실제 영상으로 결과·속도·온도 검증
  -> model bundle 배포
```

처음에는 아래 문서를 순서대로 읽습니다.

1. [ONNX 실전 흐름](onnx-workflow.md)
2. [Jetson TensorRT 변환](tensorrt-on-jetson.md)
3. [모델 bundle과 manifest](model-bundle.md)
4. 여러 카메라를 처리할 때 [DeepStream custom model](deepstream-custom-model.md)

## 어떤 실행 방식을 선택할까?

| 상황 | 권장 방식 | 이유 |
|---|---|---|
| PC에서 ONNX가 정상인지 확인 | ONNX Runtime CPU | 설치와 재현이 간단함 |
| 단일 카메라, 특수 전·후처리 | 직접 TensorRT C++ adapter | 파이프라인을 세밀하게 제어 가능 |
| 여러 카메라, tracker/OSD/encode | DeepStream `nvinfer` | NVIDIA 영상 plugin을 조합하기 쉬움 |
| TensorRT가 지원하지 않는 일부 ONNX 연산 | ONNX Runtime TensorRT EP | 지원 node는 TensorRT, 나머지는 CUDA/CPU fallback 가능 |

TensorRT와 DeepStream 중 하나가 항상 정답은 아닙니다. 첫 모델은 두 방식의 정확도, FPS, 지연, 메모리를 실제 Jetson에서 비교한 후 선택합니다.

