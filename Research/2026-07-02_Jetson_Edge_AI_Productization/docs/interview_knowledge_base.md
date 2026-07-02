# Edge AI 면접 꼬리 질문 지식 베이스

이 문서는 Jetson Orin Nano + 카메라 + 객체 탐지 + ONNX/TensorRT 프로젝트를 기반으로, 면접에서 나올 수 있는 꼬리 질문을 예상하고 답변 깊이를 키우기 위한 자료입니다.

핵심 목표는 다음 한 문장을 기술적으로 증명하는 것입니다.

> 저는 AI 모델을 단순히 실행하는 수준을 넘어, 카메라 입력부터 모델 변환, 엣지 최적화, 성능 측정, 실패 분석까지 제품화 관점으로 연결할 수 있습니다.

## 1. 전체 프로젝트 설명

### 기본 질문

Q. 이 프로젝트를 한 문장으로 설명해보세요.

답변 포인트:

- Jetson Orin Nano와 카메라 모듈을 사용해 실시간 객체 탐지 시스템을 구성했다.
- pretrained YOLO 모델을 baseline으로 사용하고, 필요하면 custom dataset으로 fine-tuning한다.
- PyTorch 모델을 ONNX로 export하고 TensorRT로 최적화한다.
- FPS, latency, memory, false positive, false negative를 측정해 제품 적용 가능성을 검증한다.

꼬리 질문:

- 왜 Jetson Orin Nano를 선택했나요?
- 왜 YOLO를 baseline으로 잡았나요?
- 단순 demo와 제품화 pipeline의 차이는 무엇인가요?
- 이 프로젝트에서 가장 큰 병목은 어디였나요?
- 정확도와 속도 중 어떤 것을 우선해야 하나요?

좋은 답변 방향:

```text
Jetson Orin Nano는 실제 엣지 디바이스 제약을 경험하기 좋습니다. GPU가 있지만 서버 GPU만큼 여유롭지 않기 때문에 모델 크기, 입력 해상도, 전처리, 후처리, TensorRT 최적화가 모두 성능에 영향을 줍니다. 그래서 모델 정확도뿐 아니라 end-to-end latency와 안정성을 같이 봐야 합니다.
```

## 2. Jetson / Edge Device

### 기본 질문

Q. Jetson에서 AI 모델을 돌릴 때 PC와 가장 다른 점은 무엇인가요?

답변 포인트:

- 전력, 발열, 메모리, CPU/GPU 자원이 제한된다.
- 카메라 입력과 inference가 같은 장치에서 동시에 실행된다.
- PyTorch 실행보다 TensorRT 최적화가 중요하다.
- 장시간 실행 안정성과 리소스 누수가 중요하다.

꼬리 질문:

- `nvpmodel`과 `jetson_clocks`는 왜 사용하나요?
- `tegrastats`로 무엇을 확인하나요?
- GPU 사용률이 낮은데 FPS도 낮으면 어디를 의심하나요?
- CPU 사용률이 높은 경우 어떤 최적화를 고려하나요?
- 메모리가 부족하면 어떤 선택지가 있나요?

답변 포인트:

```text
nvpmodel은 Jetson의 전력 모드를 설정하고, jetson_clocks는 클럭을 고정해 성능 측정 조건을 안정화합니다. tegrastats는 CPU/GPU 사용률, 메모리, 온도, 전력 상태를 확인하는 데 사용합니다. GPU 사용률이 낮고 FPS도 낮다면 모델 자체보다 카메라 입력, resize/normalize, frame copy, Python loop, postprocessing 같은 CPU 병목을 먼저 의심합니다.
```

### 추가 심화 질문

Q. Edge AI에서 "end-to-end latency"와 "model latency"는 어떻게 다른가요?

답변 포인트:

- model latency는 모델 추론 시간만 의미한다.
- end-to-end latency는 frame capture, decode, resize, normalize, inference, NMS, overlay, logging까지 포함한다.
- 제품 체감 성능은 end-to-end latency에 더 가깝다.

## 3. Camera / GStreamer / OpenCV

### 기본 질문

Q. Jetson CSI 카메라를 OpenCV에서 열 때 왜 GStreamer pipeline을 쓰나요?

답변 포인트:

- CSI 카메라는 NVIDIA Argus camera stack을 통해 접근하는 경우가 많다.
- `nvarguscamerasrc`를 사용하면 Jetson 하드웨어 경로를 활용할 수 있다.
- OpenCV의 단순 `VideoCapture(0)`은 USB UVC 카메라에는 맞지만 CSI에는 실패할 수 있다.

꼬리 질문:

- USB 카메라와 CSI 카메라의 차이는 무엇인가요?
- RTSP stream을 처리할 때 병목은 어디서 생기나요?
- 카메라 FPS와 inference FPS가 다르면 어떻게 처리하나요?
- frame drop은 언제 허용할 수 있나요?
- 실시간 시스템에서 queue가 무한히 쌓이면 어떤 문제가 생기나요?

좋은 답변 방향:

```text
실시간 카메라 시스템에서는 모든 프레임을 처리하는 것보다 최신 프레임을 일정 latency 안에 처리하는 것이 더 중요할 수 있습니다. inference가 카메라 FPS를 따라가지 못하면 queue가 계속 쌓여 화면은 늦게 보이고 메모리도 증가합니다. 이 경우 bounded queue, frame drop, inference thread 분리, 입력 해상도 조정 등을 고려합니다.
```

## 4. Object Detection / YOLO

### 기본 질문

Q. Object detection은 classification과 무엇이 다른가요?

답변 포인트:

- classification은 이미지 전체의 class를 예측한다.
- detection은 객체 class와 위치 bounding box를 함께 예측한다.
- detection은 IoU, NMS, mAP 같은 별도 개념이 필요하다.

꼬리 질문:

- Bounding box regression은 무엇인가요?
- IoU는 어떻게 계산하나요?
- NMS는 왜 필요한가요?
- confidence threshold를 올리면 어떤 일이 생기나요?
- 작은 객체 탐지가 어려운 이유는 무엇인가요?

좋은 답변 방향:

```text
confidence threshold를 올리면 낮은 확신도의 box가 제거되어 false positive는 줄 수 있지만, 실제 객체도 제거되어 false negative가 늘 수 있습니다. CCTV 도메인에서는 오탐과 미탐의 비용이 다르기 때문에 threshold는 metric과 실제 운영 상황을 같이 보고 정해야 합니다.
```

### 심화 질문

Q. YOLO 계열 모델이 실시간 탐지에 많이 쓰이는 이유는 무엇인가요?

답변 포인트:

- one-stage detector라서 inference가 빠르다.
- 학습, 평가, export workflow가 잘 갖춰져 있다.
- TensorRT/ONNX 변환 사례가 많다.
- 작은 모델부터 큰 모델까지 선택지가 있다.

꼬리 질문:

- one-stage와 two-stage detector의 차이는 무엇인가요?
- YOLO에서 backbone, neck, head는 각각 무슨 역할인가요?
- anchor-based와 anchor-free 방식의 차이는 무엇인가요?
- NMS가 병목이 될 수 있나요?
- YOLO 모델을 제품에 넣을 때 가장 조심할 점은 무엇인가요?

답변 포인트:

```text
Backbone은 이미지에서 feature를 추출하고, neck은 여러 scale의 feature를 융합하며, head는 class와 bounding box를 예측합니다. 제품 적용 시에는 모델 accuracy만 보지 않고 전처리/후처리 포함 latency, 입력 해상도, 작은 객체 탐지 성능, 오탐/미탐 패턴, 변환 가능성까지 봐야 합니다.
```

## 5. Dataset / Labeling / Evaluation

### 기본 질문

Q. 좋은 detection dataset을 만들기 위해 무엇을 봐야 하나요?

답변 포인트:

- class 정의가 명확해야 한다.
- train/val/test 분리가 적절해야 한다.
- 실제 운영 환경과 유사한 이미지가 포함되어야 한다.
- 조명, 각도, 거리, occlusion, 해상도 다양성이 필요하다.
- label 품질이 모델 성능의 상한을 만든다.

꼬리 질문:

- train set과 validation set이 너무 비슷하면 어떤 문제가 생기나요?
- class imbalance는 어떻게 해결하나요?
- 라벨링 기준이 흔들리면 어떤 문제가 생기나요?
- mAP가 높은데 실제 demo가 별로면 무엇을 의심하나요?
- false positive와 false negative를 어떻게 분석하나요?

좋은 답변 방향:

```text
mAP가 높아도 실제 demo가 나쁘다면 validation set이 운영 환경을 충분히 대표하지 못했을 가능성이 있습니다. 예를 들어 특정 조명, 카메라 각도, 작은 객체, motion blur, occlusion 상황이 validation에 부족하면 metric과 실제 체감 성능이 달라질 수 있습니다.
```

### 평가 지표 질문

Q. Precision과 Recall은 어떻게 다르고, 보안 카메라에서는 어느 쪽이 더 중요한가요?

답변 포인트:

- Precision: 탐지했다고 한 것 중 실제 정답 비율
- Recall: 실제 객체 중 놓치지 않고 탐지한 비율
- 보안 이벤트에서는 miss 비용이 크면 recall을 우선할 수 있다.
- 알람 피로도가 문제면 precision도 중요하다.
- 운영 정책에 따라 threshold와 후처리 기준을 조정한다.

꼬리 질문:

- F1 score는 언제 유용한가요?
- mAP50과 mAP50-95는 어떻게 다른가요?
- IoU threshold가 높아지면 어떤 모델이 유리한가요?
- confusion matrix를 detection에서 어떻게 활용하나요?

## 6. Fine-tuning / Transfer Learning

### 기본 질문

Q. 처음부터 모델을 학습하지 않고 pretrained 모델을 fine-tuning하는 이유는 무엇인가요?

답변 포인트:

- 작은 데이터셋에서도 빠르게 수렴한다.
- backbone이 일반적인 visual feature를 이미 학습했다.
- 학습 비용과 시간이 줄어든다.
- domain-specific data로 마지막 성능을 맞출 수 있다.

꼬리 질문:

- 언제 layer freeze를 고려하나요?
- overfitting은 어떻게 확인하나요?
- augmentation은 언제 도움이 되고 언제 해가 되나요?
- epoch를 늘리면 항상 좋아지나요?
- learning rate가 너무 크거나 작으면 어떤 현상이 생기나요?

좋은 답변 방향:

```text
fine-tuning에서는 train loss만 보는 것이 위험합니다. validation metric이 좋아지지 않거나 오히려 떨어지면 overfitting을 의심해야 합니다. 작은 데이터셋에서는 augmentation, early stopping, class balance, validation split 품질을 같이 봐야 합니다.
```

## 7. ONNX

### 기본 질문

Q. ONNX는 왜 사용하나요?

답변 포인트:

- PyTorch 같은 학습 프레임워크와 실제 inference runtime을 분리한다.
- C++/C#/Python 등 다양한 환경에서 모델 실행이 가능해진다.
- TensorRT 최적화의 입력 포맷으로 자주 사용된다.
- 제품화에서는 학습 코드 의존성을 줄이는 데 도움이 된다.

꼬리 질문:

- ONNX export에서 opset은 무엇인가요?
- dynamic shape와 static shape는 어떤 차이가 있나요?
- ONNX Runtime 결과와 PyTorch 결과가 다르면 무엇을 확인하나요?
- preprocessing과 postprocessing은 ONNX 안에 넣어야 하나요?
- unsupported operator가 나오면 어떻게 해결하나요?

좋은 답변 방향:

```text
ONNX 변환 후에는 PyTorch 출력과 ONNX Runtime 출력을 같은 입력으로 비교해야 합니다. 차이가 크면 preprocessing, input layout, dtype, normalization, opset, dynamic shape, postprocessing 구현 차이를 확인합니다. 제품에서는 재현 가능한 입력 샘플을 고정해 regression test처럼 비교하는 것이 좋습니다.
```

## 8. TensorRT / Quantization

### 기본 질문

Q. TensorRT를 사용하면 왜 빨라지나요?

답변 포인트:

- NVIDIA GPU에 맞게 graph를 최적화한다.
- layer fusion, kernel auto-tuning, memory optimization을 수행한다.
- FP16/INT8 같은 낮은 precision을 활용할 수 있다.
- Jetson에서는 PyTorch보다 inference overhead가 줄어든다.

꼬리 질문:

- FP32와 FP16의 차이는 무엇인가요?
- INT8 quantization에는 왜 calibration dataset이 필요한가요?
- TensorRT engine은 다른 Jetson에서도 그대로 쓸 수 있나요?
- TensorRT 변환 후 정확도가 떨어지면 어떻게 분석하나요?
- `trtexec` 결과에서 어떤 값을 봐야 하나요?

좋은 답변 방향:

```text
FP16은 표현 정밀도를 낮춰 연산량과 메모리 사용량을 줄이는 방식입니다. 일반적인 CNN detection 모델에서는 성능 저하가 작고 속도 개선이 큰 경우가 많습니다. INT8은 더 빠를 수 있지만 activation range를 잡기 위한 calibration이 필요하고, 정확도 손실을 반드시 검증해야 합니다.
```

### 심화 질문

Q. TensorRT engine 파일을 배포할 때 주의할 점은 무엇인가요?

답변 포인트:

- engine은 GPU architecture, TensorRT version, build option에 종속될 수 있다.
- target device에서 build하거나 device별 engine을 관리하는 편이 안전하다.
- input shape, precision, plugin dependency를 명확히 기록해야 한다.

## 9. Performance Profiling

### 기본 질문

Q. FPS가 낮을 때 어떤 순서로 병목을 찾겠습니까?

답변 포인트:

1. 카메라 입력 FPS 확인
2. resize/normalize 전처리 시간 측정
3. model inference latency 측정
4. NMS/postprocessing 시간 측정
5. overlay/display/logging 시간 측정
6. CPU/GPU/memory 사용률 확인

꼬리 질문:

- 평균 latency만 보면 왜 위험한가요?
- p95 latency는 왜 보나요?
- throughput과 latency는 어떻게 다른가요?
- batch size를 키우면 항상 좋은가요?
- display를 끄면 FPS가 올라가는 이유는 무엇인가요?

좋은 답변 방향:

```text
평균 latency는 순간적인 지연을 가릴 수 있습니다. 실시간 카메라에서는 가끔 튀는 latency도 사용자 체감이나 이벤트 처리에 영향을 줍니다. 그래서 mean, median뿐 아니라 p95, max를 같이 봅니다.
```

## 10. Productization / Reliability

### 기본 질문

Q. 연구 코드와 제품 코드의 차이는 무엇이라고 보나요?

답변 포인트:

- 연구 코드는 성능 검증과 실험 유연성이 중요하다.
- 제품 코드는 안정성, 재현성, 관측 가능성, 장애 대응이 중요하다.
- config, logging, metric, error handling, resource cleanup이 필요하다.
- 장시간 실행과 예외 상황을 고려해야 한다.

꼬리 질문:

- 카메라 연결이 끊기면 어떻게 처리하나요?
- 모델 파일이 잘못되었을 때 어떻게 감지하나요?
- inference 결과를 어떻게 로그로 남기나요?
- 장시간 실행 테스트는 어떻게 설계하나요?
- 모델 버전 관리는 어떻게 하나요?

좋은 답변 방향:

```text
제품 코드에서는 정상 입력만 가정하면 안 됩니다. 카메라 reconnect, frame read 실패, 모델 파일 mismatch, GPU memory 부족, queue overflow 같은 상황을 처리해야 합니다. 또한 model version, config, runtime latency, event log를 남겨야 현장 이슈를 분석할 수 있습니다.
```

## 11. C++ / C# Runtime 확장

### 기본 질문

Q. Python으로 동작하는데 왜 C++ 또는 C# inference를 고려하나요?

답변 포인트:

- 기존 제품 코드가 C++/C# 기반일 수 있다.
- GUI, device control, service integration과 연결해야 한다.
- Python runtime 의존성을 줄이고 배포를 단순화할 수 있다.
- latency와 resource control이 더 중요할 수 있다.

꼬리 질문:

- ONNX Runtime C++ API를 쓸 때 핵심 단계는 무엇인가요?
- C#에서 ONNX Runtime을 쓸 때 preprocessing은 어디서 처리하나요?
- Python 결과와 C++ 결과를 어떻게 검증하나요?
- OpenCV Mat과 model input tensor 변환에서 주의할 점은 무엇인가요?

좋은 답변 방향:

```text
Python과 C++/C# 결과 비교는 같은 이미지, 같은 resize 방식, 같은 normalization, 같은 channel order, 같은 threshold/NMS 조건으로 해야 합니다. 특히 BGR/RGB, NHWC/NCHW, uint8/float32 변환 차이가 결과 불일치의 흔한 원인입니다.
```

## 12. VLM / LLM 확장

### 기본 질문

Q. 객체 탐지 프로젝트를 VLM/LLM과 어떻게 연결할 수 있나요?

답변 포인트:

- detection 결과를 structured context로 만든다.
- key frame과 detection JSON을 VLM에 함께 입력한다.
- "무엇이 보이는가"를 넘어 "상황 설명"과 "이벤트 요약"으로 확장한다.
- CCTV 도메인 instruction dataset으로 LoRA fine-tuning을 고려할 수 있다.

꼬리 질문:

- VLM이 hallucination을 일으키면 어떻게 줄이나요?
- detection 결과와 VLM 설명이 충돌하면 무엇을 신뢰하나요?
- LoRA fine-tuning은 왜 쓰나요?
- VLM을 Jetson에서 직접 돌릴 수 있나요?
- 개인정보/보안 이슈는 어떻게 고려하나요?

좋은 답변 방향:

```text
VLM은 장면 설명에는 강점이 있지만, 제품에서는 근거 없는 설명을 줄여야 합니다. detection 결과, timestamp, ROI 같은 구조화된 정보를 함께 제공하고, 답변 형식을 제한하면 hallucination을 줄일 수 있습니다. 무거운 VLM은 Jetson에서 직접 실행하기 어려울 수 있어 edge에서는 detection과 key frame 추출을 수행하고, 서버에서 VLM 분석을 하는 hybrid 구조도 고려할 수 있습니다.
```

## 13. 압박 질문 대응

### Q. 이건 그냥 YOLO 예제 실행 아닌가요?

답변 방향:

```text
pretrained YOLO 실행은 시작점일 뿐입니다. 제가 집중한 부분은 Jetson 카메라 입력, FPS/latency 측정, ONNX export, TensorRT 최적화, false positive/false negative 분석, 데이터셋 개선 루프까지 연결하는 것입니다. 즉 모델 demo가 아니라 제품 적용 가능성을 검증하는 pipeline을 만드는 것이 목표였습니다.
```

### Q. 직접 모델 구조를 만든 것은 아니네요?

답변 방향:

```text
맞습니다. 이 프로젝트의 1차 목표는 새로운 architecture 제안이 아니라 edge AI productization 역량을 증명하는 것입니다. 실제 제품에서는 검증된 baseline을 빠르게 적용하고, 데이터와 최적화, 운영 제약을 해결하는 능력도 중요합니다. 이후 단계에서는 detection failure case를 기반으로 model architecture나 loss, data strategy를 개선하는 방향으로 확장할 수 있습니다.
```

### Q. 정확도보다 FPS만 본 것 아닌가요?

답변 방향:

```text
FPS만 보면 위험합니다. 그래서 precision, recall, mAP 같은 offline metric과 함께 실제 카메라 환경에서 false positive/false negative를 수집하고 분석합니다. edge 제품에서는 accuracy와 latency가 trade-off 관계에 있으므로 둘을 같이 보고 운영 threshold와 모델 크기를 결정해야 합니다.
```

### Q. Jetson에서 VLM까지 돌릴 수 있나요?

답변 방향:

```text
소형 VLM이나 quantized model은 가능성을 검토할 수 있지만, Orin Nano에서는 실시간 카메라 처리와 VLM을 동시에 안정적으로 돌리는 데 제약이 큽니다. 현실적인 구조는 edge에서 detection, tracking, key frame extraction을 수행하고, 필요 이벤트만 서버나 더 큰 GPU로 보내 VLM 설명을 생성하는 hybrid 방식입니다.
```

## 14. 스스로 점검할 질문

아래 질문에 막힘없이 답할 수 있으면 프로젝트 설명의 밀도가 올라갑니다.

- 내 pipeline의 end-to-end latency는 몇 ms인가?
- PyTorch, ONNX Runtime, TensorRT의 latency 차이는 얼마인가?
- 카메라 입력 FPS와 inference FPS는 각각 얼마인가?
- confidence threshold를 바꾸면 오탐/미탐이 어떻게 변했는가?
- 가장 흔한 false positive 원인은 무엇인가?
- 가장 흔한 false negative 원인은 무엇인가?
- TensorRT FP16 변환 후 정확도 손실은 있었는가?
- 모델 입력 해상도 640과 320의 차이는 무엇이었는가?
- CPU 병목과 GPU 병목을 어떻게 구분했는가?
- 이 프로젝트를 C++/C# 제품 코드로 옮긴다면 어디부터 바꿀 것인가?

