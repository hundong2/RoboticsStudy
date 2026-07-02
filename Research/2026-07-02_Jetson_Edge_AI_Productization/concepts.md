# 핵심 기술 지식

## 1. Edge AI Productization

제품화 관점에서는 모델 정확도 하나만으로 충분하지 않습니다. 실제 카메라 제품에서는 아래 요소가 함께 맞아야 합니다.

```text
입력 안정성
전처리 비용
추론 latency
후처리 비용
FPS
GPU/CPU 사용률
메모리 사용량
오탐/미탐 패턴
장시간 실행 안정성
배포 포맷
```

따라서 이 프로젝트의 핵심은 "모델을 학습했다"가 아니라 "모델을 실제 디바이스에서 반복 실행하고 측정했다"입니다.

## 2. Object Detection

객체 탐지는 이미지 안에서 물체의 종류와 위치를 찾는 작업입니다.

주요 개념:

```text
Bounding Box: 객체 위치를 나타내는 사각형
Class: person, car 같은 객체 종류
Confidence: 모델이 예측을 얼마나 확신하는지 나타내는 점수
IoU: 예측 box와 정답 box가 얼마나 겹치는지
NMS: 중복 box를 제거하는 후처리
mAP: detection 성능을 요약하는 대표 지표
```

CCTV/보안 도메인에서는 특히 false positive와 false negative가 중요합니다.

- false positive: 실제로는 없는데 있다고 탐지
- false negative: 실제로 있는데 놓침

제품에서는 "mAP가 높다"보다 "어떤 상황에서 왜 오탐/미탐이 나는지 설명할 수 있다"가 더 중요할 때가 많습니다.

## 3. YOLO

YOLO 계열 모델은 실시간 객체 탐지에 자주 쓰입니다.

장점:

- 빠른 inference
- 학습/검증/export 도구가 잘 정리되어 있음
- ONNX/TensorRT로 이동하기 쉬움

학습 포인트:

```text
imgsz를 키우면 작은 객체 탐지는 좋아질 수 있지만 latency가 증가합니다.
conf threshold를 높이면 오탐은 줄지만 미탐이 늘 수 있습니다.
NMS threshold는 중복 box 제거에 영향을 줍니다.
작은 데이터셋에서는 augmentation과 validation split이 중요합니다.
```

## 4. ONNX

ONNX는 학습 프레임워크와 실행 런타임을 분리하기 위한 중간 모델 포맷입니다.

```text
PyTorch model -> ONNX -> ONNX Runtime / TensorRT / C++ / C#
```

제품화 관점에서 ONNX를 익혀야 하는 이유:

- Python 학습 코드와 실제 서비스 코드를 분리할 수 있음
- C++/C# 런타임으로 연결하기 쉬움
- TensorRT 최적화의 입력 포맷으로 자주 사용됨

주의할 점:

- dynamic shape는 편하지만 최적화가 어려울 수 있음
- 일부 operator는 TensorRT에서 지원되지 않을 수 있음
- preprocessing/postprocessing이 모델 밖에 있을 수 있음

## 5. TensorRT

TensorRT는 NVIDIA GPU에서 inference를 빠르게 실행하기 위한 최적화 런타임입니다.

대표 최적화:

```text
FP32 -> FP16
INT8 quantization
layer fusion
kernel auto-tuning
memory optimization
```

Jetson에서는 TensorRT 경험이 매우 중요합니다. 같은 모델이라도 PyTorch로 직접 실행하는 것과 TensorRT engine으로 실행하는 것은 FPS와 latency가 크게 다를 수 있습니다.

## 6. GStreamer와 Camera Pipeline

Jetson CSI 카메라는 일반 USB 카메라처럼 단순히 `/dev/video0`로만 처리되지 않을 수 있습니다. NVIDIA의 `nvarguscamerasrc`를 사용하는 GStreamer pipeline을 이해해야 합니다.

대표 CSI pipeline:

```text
nvarguscamerasrc
-> video/x-raw(memory:NVMM)
-> nvvidconv
-> video/x-raw, format=BGRx
-> videoconvert
-> appsink
```

이 흐름을 이해하면 OpenCV에서 카메라 입력 문제를 디버깅하기 쉬워집니다.

## 7. VLM/LLM 확장 방향

이 프로젝트를 마친 뒤에는 VLM을 붙여 "탐지 결과를 설명하는 시스템"으로 확장할 수 있습니다.

예:

```text
Detection: person 2명, car 1대
VLM/LLM output: 화면 중앙에 보행자 2명이 있고, 오른쪽 하단에 차량이 접근 중입니다.
```

초기 확장 과제:

- key frame 추출
- detection 결과를 JSON으로 저장
- VLM에 이미지와 detection context를 함께 입력
- CCTV 이벤트 설명 dataset 구성
- LoRA fine-tuning 실험

