# 09. 온디바이스 추론 최적화: ONNX Runtime, TensorRT

목표는 PyTorch로 학습한 policy를 로봇 보드에서 빠르고 안정적으로 실행하는 것입니다.

## 강의 목표

- PyTorch checkpoint와 inference model artifact를 구분한다.
- ONNX export와 ONNX Runtime 실행을 이해한다.
- TensorRT engine, precision, dynamic shape, calibration의 의미를 설명한다.
- latency, throughput, memory, power를 측정한다.

## 배포 흐름

```text
PyTorch training checkpoint
  -> eval mode + preprocessing freeze
  -> ONNX export
  -> ONNX Runtime benchmark
  -> TensorRT engine build
  -> device latency/power test
  -> ROS 2 inference node
```

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | inference-only model 분리 | `PolicyInference` class |
| 2 | preprocessing freeze | normalization config |
| 3 | ONNX export | `.onnx` 파일 |
| 4 | ONNX Runtime CPU benchmark | latency table |
| 5 | GPU provider 조사 | provider memo |
| 6 | TensorRT 개념 정리 | precision 비교 |
| 7 | FP32/FP16/INT8 trade-off | accuracy-latency table |
| 8 | ROS 2 inference node 설계 | node diagram |
| 9 | memory profiling | memory report |
| 10 | deployment report | artifact manifest |

## 실습 과제

1. 학습한 BC policy를 ONNX로 export한다.
2. PyTorch와 ONNX Runtime 출력 차이를 비교한다.
3. latency p50/p90/p99를 측정한다.
4. 모델, preprocessing, action scale, ROS message type을 artifact manifest에 기록한다.

## 통과 기준

- training code와 inference code를 분리해야 하는 이유를 설명한다.
- dynamic shape가 편리하지만 최적화에 불리할 수 있음을 설명한다.
- latency 평균보다 p99가 중요한 상황을 예로 든다.
