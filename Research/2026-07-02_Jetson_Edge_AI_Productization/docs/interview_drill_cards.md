# 면접 드릴 카드

이 문서는 반복 연습용입니다. 각 질문에 대해 60초 답변을 먼저 말하고, 그 다음 꼬리 질문 3개까지 이어서 답변해보세요.

## Card 1. 프로젝트 개요

질문:

```text
Jetson Orin Nano 프로젝트를 설명해보세요.
```

60초 답변 구조:

```text
1. Jetson + 카메라 기반 실시간 객체 탐지 시스템입니다.
2. YOLO baseline으로 카메라 입력을 처리했습니다.
3. custom dataset 수집과 fine-tuning까지 확장 가능하게 구성했습니다.
4. ONNX/TensorRT 변환으로 edge inference 최적화를 실습했습니다.
5. FPS, latency, 오탐/미탐을 측정해 제품화 관점으로 평가했습니다.
```

꼬리 질문:

- 왜 이 주제를 선택했나요?
- 업무와 어떤 관련이 있나요?
- 이 프로젝트에서 본인의 기존 강점은 어디에 녹아 있나요?

## Card 2. Edge AI

질문:

```text
서버 GPU가 아니라 edge device에서 AI를 돌릴 때 중요한 점은 무엇인가요?
```

핵심 키워드:

```text
전력, 발열, 메모리, latency, 카메라 입력, TensorRT, 장시간 안정성
```

꼬리 질문:

- Jetson에서 성능 측정 조건을 어떻게 고정하나요?
- GPU 사용률이 낮은데 느리면 어디를 봐야 하나요?
- end-to-end latency는 어떻게 측정하나요?

## Card 3. Camera Pipeline

질문:

```text
Jetson CSI 카메라를 OpenCV에서 처리할 때 왜 GStreamer를 사용하나요?
```

핵심 키워드:

```text
nvarguscamerasrc, Argus, NVMM, hardware path, appsink
```

꼬리 질문:

- USB 카메라와 CSI 카메라는 무엇이 다른가요?
- inference FPS가 camera FPS보다 낮으면 어떻게 하나요?
- frame queue를 무한정 쌓으면 왜 안 되나요?

## Card 4. YOLO

질문:

```text
YOLO가 실시간 객체 탐지에 적합한 이유는 무엇인가요?
```

핵심 키워드:

```text
one-stage detector, latency, backbone/neck/head, export workflow
```

꼬리 질문:

- NMS는 왜 필요한가요?
- confidence threshold를 올리면 어떤 trade-off가 생기나요?
- 작은 객체 탐지는 왜 어렵나요?

## Card 5. Dataset

질문:

```text
객체 탐지 데이터셋을 만들 때 가장 중요한 것은 무엇인가요?
```

핵심 키워드:

```text
class definition, label consistency, train/val split, domain coverage, failure cases
```

꼬리 질문:

- validation 성능은 좋은데 실제 카메라에서 나쁘면 왜 그럴까요?
- class imbalance는 어떻게 다루나요?
- 오탐/미탐 이미지는 어떻게 활용하나요?

## Card 6. Metric

질문:

```text
Precision과 Recall의 차이를 설명해보세요.
```

핵심 키워드:

```text
false positive, false negative, alarm fatigue, miss cost, threshold
```

꼬리 질문:

- 보안 카메라에서는 precision과 recall 중 무엇이 더 중요한가요?
- mAP50과 mAP50-95는 어떻게 다른가요?
- F1 score는 언제 유용한가요?

## Card 7. ONNX

질문:

```text
PyTorch 모델을 ONNX로 변환하는 이유는 무엇인가요?
```

핵심 키워드:

```text
runtime separation, C++/C# deployment, TensorRT input, reproducibility
```

꼬리 질문:

- dynamic shape와 static shape는 무엇이 다른가요?
- ONNX 변환 후 결과가 달라지면 무엇을 확인하나요?
- preprocessing은 모델 안에 넣는 것이 좋나요, 밖에 두는 것이 좋나요?

## Card 8. TensorRT

질문:

```text
TensorRT 최적화는 어떤 원리로 빨라지나요?
```

핵심 키워드:

```text
layer fusion, kernel tuning, FP16, INT8, memory optimization
```

꼬리 질문:

- FP16과 INT8의 차이는 무엇인가요?
- INT8 calibration dataset은 왜 필요한가요?
- TensorRT engine 배포 시 주의할 점은 무엇인가요?

## Card 9. Profiling

질문:

```text
FPS가 낮으면 어떤 순서로 분석하겠습니까?
```

핵심 키워드:

```text
capture, preprocessing, inference, NMS, rendering, logging, tegrastats
```

꼬리 질문:

- 평균 latency만 보면 왜 부족한가요?
- p95 latency는 왜 중요합니까?
- display를 끄면 FPS가 오르는 이유는 무엇인가요?

## Card 10. Productization

질문:

```text
연구 코드와 제품 코드는 무엇이 다르다고 생각하나요?
```

핵심 키워드:

```text
error handling, logging, config, versioning, resource cleanup, long-run test
```

꼬리 질문:

- 카메라 연결이 끊기면 어떻게 복구하나요?
- 모델 버전은 어떻게 관리하나요?
- 장시간 실행 안정성은 어떻게 검증하나요?

## Card 11. C++ / C#

질문:

```text
Python demo를 C++/C# 제품 코드로 옮길 때 무엇을 조심해야 하나요?
```

핵심 키워드:

```text
BGR/RGB, NHWC/NCHW, dtype, normalization, NMS consistency, ONNX Runtime
```

꼬리 질문:

- Python과 C++ 결과가 다르면 어떻게 비교하나요?
- OpenCV Mat을 tensor로 바꿀 때 주의할 점은 무엇인가요?
- C#에서 ONNX Runtime을 쓰는 장점은 무엇인가요?

## Card 12. VLM 확장

질문:

```text
객체 탐지 결과를 VLM/LLM과 어떻게 연결할 수 있나요?
```

핵심 키워드:

```text
key frame, detection JSON, structured context, event summary, hallucination control
```

꼬리 질문:

- VLM hallucination을 어떻게 줄이나요?
- Jetson에서 VLM까지 직접 돌리는 것이 현실적인가요?
- edge-server hybrid 구조는 언제 필요하나요?

