# 05. 효율화, 병렬화, 서빙 용어

## 모델 압축과 연산 최적화

| 용어 | 의미 | trade-off |
|---|---|---|
| Quantization | FP16/FP32 대신 INT8, INT4 등 낮은 precision 사용 | memory와 속도는 개선, 품질 손실 가능 |
| Pruning | 중요도가 낮은 weight, head, neuron 제거 | 작아지지만 재학습이 필요할 수 있음 |
| Distillation | 큰 teacher model의 행동을 작은 student model에 학습 | 작고 빠르지만 teacher 한계를 물려받음 |
| Sparsity | 0 또는 비활성 요소가 많은 구조 활용 | hardware 지원 여부가 중요 |
| Kernel fusion | 여러 GPU 연산을 하나로 합침 | launch overhead와 memory 이동 감소 |
| FlashAttention | attention 계산의 memory IO를 줄인 kernel | 긴 sequence에서 효과적 |

## 병렬화

| 용어 | 의미 | 언제 쓰나 |
|---|---|---|
| Data parallelism | 같은 모델을 여러 GPU에 복제하고 데이터를 나눔 | 학습 batch 확대 |
| Tensor parallelism | 한 layer의 행렬 연산을 여러 GPU에 나눔 | 큰 모델 추론/학습 |
| Pipeline parallelism | layer 묶음을 여러 GPU에 나눔 | 모델이 한 GPU에 안 들어갈 때 |
| Expert parallelism | MoE expert를 여러 장치에 분산 | sparse MoE 모델 |
| Sequence parallelism | sequence 차원을 나눠 처리 | 긴 context 학습 |

## 서빙 시스템

| 용어 | 의미 | 논문 독해 포인트 |
|---|---|---|
| Serving | 모델을 API나 서비스로 운영하는 전체 시스템 | latency, throughput, reliability를 봅니다. |
| Scheduler | 요청을 어떤 순서와 batch로 처리할지 정함 | online serving 성능의 핵심입니다. |
| Batching | 여러 요청을 묶어 한 번에 처리 | throughput은 늘지만 latency가 늘 수 있습니다. |
| Dynamic batching | 실시간 요청을 짧은 대기 후 묶음 | 서버 효율과 응답성의 균형입니다. |
| Load balancing | 여러 GPU/서버에 요청 분산 | hot spot을 줄입니다. |
| Autoscaling | 부하에 따라 worker 수 자동 조정 | 비용과 안정성 trade-off입니다. |
| Cold start | 새 worker가 준비되기 전 지연 | 큰 모델 로딩에서 치명적입니다. |

## Hardware 관련 용어

| 용어 | 의미 |
|---|---|
| GPU memory | parameter, activation, KV cache가 올라가는 메모리 |
| HBM | GPU에 붙은 고대역폭 메모리 |
| FLOPs | 부동소수점 연산 수 |
| TFLOPS | 초당 테라 단위 FLOPs |
| Memory bandwidth | 초당 메모리 이동량 |
| Interconnect | GPU 간 통신 경로. NVLink, PCIe 등 |

## 30초 기억 문장

LLM 시스템 논문은 대부분 "연산을 줄일 것인가", "메모리 이동을 줄일 것인가", "여러 요청을 더 잘 섞을 것인가"의 조합이다. 성능 수치를 볼 때는 latency, throughput, memory, quality가 동시에 좋아졌는지 확인해야 한다.

