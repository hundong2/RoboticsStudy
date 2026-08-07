# 06. 임베디드 추론 최적화

로봇 보드는 서버보다 느리고, 전력과 발열 제약이 큽니다. [임베디드 최적화](../glossary/README.md#embedded-optimization)의 목표는 모델을 "작게" 만드는 것만이 아니라 제어 주기 안에 안정적으로 끝나게 만드는 것입니다.

## 최적화 흐름

```text
PyTorch model
  -> ONNX export
  -> ONNX Runtime benchmark
  -> TensorRT engine
  -> ROS 2 C++ node integration
  -> real device p50/p90/p99 latency report
```

## 측정 먼저

[TensorRT](../glossary/README.md#tensorrt) 공식 best practice도 측정과 최적화를 반복하는 흐름을 강조합니다. 추측으로 최적화하면 엉뚱한 곳을 고치기 쉽습니다.

| 항목 | 측정 이유 |
|---|---|
| p50 latency | 일반적인 응답 속도 |
| p90/p99 latency | 위험한 tail latency |
| throughput | batch 처리 성능 |
| memory | OOM과 swap 방지 |
| power/temperature | throttle 확인 |
| accuracy drift | FP16/INT8 변환 후 품질 저하 확인 |

## ONNX Runtime과 TensorRT 선택

| 도구 | 장점 | 주의 |
|---|---|---|
| [ONNX Runtime](../glossary/README.md#onnx-runtime) | 플랫폼 폭이 넓고 C++ 통합이 좋음 | execution provider 설정 확인 |
| [TensorRT](../glossary/README.md#tensorrt) | NVIDIA GPU에서 강력한 최적화 | engine build, precision, dynamic shape 관리 필요 |

## 실무 최적화 체크리스트

- [ ] preprocessing을 모델 밖/안 어디에 둘지 결정했다.
- [ ] input shape와 dtype을 고정했다.
- [ ] dynamic shape가 꼭 필요한지 확인했다.
- [ ] FP32 baseline과 FP16/INT8 결과 차이를 비교했다.
- [ ] ROS 2 callback 안에서 불필요한 memory allocation을 줄였다.
- [ ] model warmup 후 latency를 측정했다.
- [ ] p99 latency가 제어 주기보다 짧다.

## 통과 기준

- ONNX export가 성공해도 실제 속도 최적화가 끝난 것이 아님을 설명한다.
- 평균 latency와 p99 latency의 차이를 실제 로봇 안전 관점에서 설명한다.
- TensorRT engine은 모델과 하드웨어/shape/precision 조건에 묶인 artifact임을 이해한다.
