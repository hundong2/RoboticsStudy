# 면접 답변 템플릿

## 1. 30초 자기소개형 프로젝트 답변

```text
Jetson Orin Nano와 카메라 모듈을 사용해 실시간 객체 탐지 pipeline을 구축했습니다. 단순히 pretrained YOLO를 실행하는 데서 끝내지 않고, 카메라 입력, 전처리, 추론, 후처리, FPS/latency 측정, ONNX export, TensorRT 최적화까지 연결했습니다. 이 과정에서 모델 정확도뿐 아니라 edge device에서 실제 제품처럼 동작할 수 있는지 확인하는 데 초점을 맞췄습니다.
```

## 2. 60초 상세 프로젝트 답변

```text
이 프로젝트의 목표는 AI 모델을 실제 카메라 제품 환경에 올리는 과정을 경험하는 것입니다. Jetson Orin Nano에서 CSI 또는 USB 카메라 입력을 받고, YOLO 기반 객체 탐지를 실시간으로 수행합니다. 이후 직접 수집한 이미지로 custom dataset을 만들고 fine-tuning을 수행할 수 있게 구성했습니다.

제품화 관점에서는 PyTorch 모델 실행만으로는 부족하다고 보고, ONNX export와 TensorRT FP16 engine 변환까지 포함했습니다. 성능은 FPS만 보지 않고 frame capture부터 preprocessing, inference, NMS, rendering까지 포함한 end-to-end latency를 측정합니다. 또한 false positive와 false negative 이미지를 따로 분석해 데이터 보강과 threshold 조정 방향을 도출합니다.
```

## 3. 한화비전 직무 연결 답변

```text
한화비전은 영상 보안과 edge AI camera가 중요한 회사이기 때문에, 단순 모델 연구보다 실제 카메라 입력과 edge inference 환경을 이해하는 것이 중요하다고 봤습니다. 저는 기존에 C/C++/C#/Python과 임베디드, 자동화 경험이 있기 때문에, AI 모델을 학습하는 것에서 끝내지 않고 ONNX, TensorRT, 카메라 pipeline, 성능 측정, 로그 분석까지 연결하는 방향으로 역량을 쌓고 있습니다.
```

## 4. 약점 보완형 답변

```text
기존 경력은 임베디드와 제품 개발, 자동화에 강점이 있었지만, Computer Vision 모델 학습과 VLM/LLM fine-tuning 경험은 상대적으로 보완이 필요했습니다. 그래서 먼저 보안 카메라 도메인과 가까운 객체 탐지 문제를 잡고, YOLO fine-tuning, ONNX export, TensorRT 최적화, Jetson 실시간 추론까지 이어지는 실습 프로젝트를 구성했습니다. 이후에는 detection 결과와 key frame을 VLM으로 설명하는 방향까지 확장할 계획입니다.
```

## 5. 압박 질문 답변

질문:

```text
이 프로젝트가 단순 예제 실행과 다른 점은 무엇인가요?
```

답변:

```text
단순 예제는 pretrained 모델을 이미지나 웹캠에 한 번 실행하는 수준입니다. 제가 구성한 프로젝트는 Jetson 카메라 입력, 실시간 추론, 데이터 수집, fine-tuning, ONNX 변환, TensorRT 최적화, FPS/latency 측정, false positive/false negative 분석까지 하나의 반복 가능한 pipeline으로 묶는 것이 핵심입니다. 즉 demo가 아니라 제품 적용 가능성을 검증하는 흐름입니다.
```

질문:

```text
직접 새로운 모델을 만든 것은 아닌데 연구소 업무에 충분한가요?
```

답변:

```text
새로운 모델 구조를 제안하는 역량도 중요하지만, edge AI 제품에서는 검증된 모델을 실제 디바이스 제약 안에서 안정적으로 동작시키는 역량도 중요합니다. 저는 먼저 baseline 모델을 제품화 가능한 형태로 옮기는 역량을 쌓고, 그 과정에서 발견한 실패 사례와 병목을 기반으로 데이터, loss, architecture 개선으로 확장하는 접근이 현실적이라고 봅니다.
```

질문:

```text
VLM/LLM 경험은 아직 부족한 것 아닌가요?
```

답변:

```text
맞습니다. 그래서 VLM을 바로 큰 모델 fine-tuning부터 시작하기보다, 먼저 카메라 기반 detection pipeline을 안정화하고 detection 결과를 structured context로 만드는 기반을 잡고 있습니다. 그 다음 key frame과 detection JSON을 VLM에 입력해 상황 설명을 생성하고, CCTV 도메인 instruction dataset으로 LoRA fine-tuning하는 단계로 확장할 계획입니다.
```

