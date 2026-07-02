# 8주 실행 커리큘럼

목표는 "학습한 기술 목록"이 아니라 "실행 가능한 결과물"을 쌓는 것입니다. 매주 반드시 코드, 수치, 리포트를 남깁니다.

## Week 1. Jetson + Camera Bring-up

목표:

- Jetson 환경을 직접 점검한다.
- CSI/USB 카메라 입력을 OpenCV로 받는다.
- FPS 측정 습관을 만든다.

실행:

```bash
python scripts/00_check_jetson.py
python scripts/01_camera_preview.py --source csi
python scripts/01_camera_preview.py --source usb --device 0
```

학습 키워드:

```text
JetPack, CUDA, TensorRT, GStreamer, CSI, USB UVC, FPS, latency
```

산출물:

- 카메라 실행 스크린샷
- 카메라 종류별 성공/실패 기록
- `tegrastats` 캡처

## Week 2. Pretrained YOLO 실시간 추론

목표:

- pretrained detection model을 카메라 입력에 연결한다.
- FPS와 confidence threshold의 관계를 본다.

실행:

```bash
python scripts/02_yolo_camera_infer.py --source csi --model yolo11n.pt --conf 0.25
python scripts/02_yolo_camera_infer.py --source csi --model yolo11n.pt --conf 0.50
```

학습 키워드:

```text
object detection, confidence, bounding box, NMS, false positive, false negative
```

산출물:

- confidence별 탐지 결과 비교
- FPS 평균값
- 오탐/미탐 이미지 10장 저장

## Week 3. 데이터 수집과 라벨링

목표:

- 직접 카메라로 데이터를 모은다.
- detection dataset 구조를 이해한다.

실행:

```bash
python scripts/05_capture_dataset.py --source csi --output data/raw/camera --interval 10
```

라벨링 도구 후보:

- CVAT
- Label Studio
- Roboflow

YOLO dataset 구조:

```text
dataset/
  images/
    train/
    val/
  labels/
    train/
    val/
  data.yaml
```

산출물:

- 직접 촬영한 이미지 300장 이상
- 라벨링된 이미지 100장 이상
- train/val split
- class 정의 문서

## Week 4. Fine-tuning

목표:

- 작은 데이터셋으로 YOLO fine-tuning을 수행한다.
- overfitting과 class imbalance를 경험한다.

실행 예시:

```bash
yolo detect train \
  model=yolo11n.pt \
  data=data/dataset/data.yaml \
  imgsz=640 \
  epochs=50 \
  batch=8 \
  device=0 \
  project=runs/edge_ai \
  name=custom_yolo11n
```

평가:

```bash
yolo detect val \
  model=runs/edge_ai/custom_yolo11n/weights/best.pt \
  data=data/dataset/data.yaml \
  imgsz=640 \
  device=0
```

산출물:

- `best.pt`
- precision/recall/mAP 그래프
- 실패 이미지 분석
- 데이터 추가 전략

## Week 5. ONNX Export와 ONNX Runtime

목표:

- 학습 모델을 제품 런타임 친화적인 포맷으로 변환한다.
- Python 학습 코드와 inference runtime을 분리해서 생각한다.

실행:

```bash
python scripts/03_export_yolo_onnx.py \
  --model runs/edge_ai/custom_yolo11n/weights/best.pt \
  --imgsz 640

python scripts/04_benchmark_onnxruntime.py \
  --onnx runs/edge_ai/custom_yolo11n/weights/best.onnx \
  --shape 1 3 640 640 \
  --warmup 20 \
  --repeat 100
```

학습 키워드:

```text
ONNX, opset, static shape, dynamic shape, ONNX Runtime, preprocessing, postprocessing
```

산출물:

- ONNX 파일
- ONNX Runtime latency 리포트
- PyTorch 결과와 ONNX 결과 비교

## Week 6. TensorRT 최적화

목표:

- Jetson에서 TensorRT engine을 만든다.
- FP32/FP16의 성능 차이를 비교한다.

실행:

```bash
trtexec \
  --onnx=runs/edge_ai/custom_yolo11n/weights/best.onnx \
  --saveEngine=runs/edge_ai/custom_yolo11n/weights/best_fp16.engine \
  --fp16 \
  --shapes=images:1x3x640x640

trtexec \
  --loadEngine=runs/edge_ai/custom_yolo11n/weights/best_fp16.engine \
  --shapes=images:1x3x640x640
```

학습 키워드:

```text
TensorRT engine, FP16, INT8, calibration, throughput, latency, memory
```

산출물:

- `.engine` 파일
- FP32/FP16 latency 비교
- 정확도 손실 여부 기록

## Week 7. Product-style Pipeline

목표:

- 카메라 입력, inference, overlay, logging, metric 저장을 하나의 제품형 파이프라인으로 만든다.

실행:

```bash
python scripts/02_yolo_camera_infer.py \
  --source csi \
  --model runs/edge_ai/custom_yolo11n/weights/best.pt \
  --conf 0.35 \
  --save-video outputs/demo.mp4 \
  --save-metrics outputs/metrics.csv
```

추가 구현 과제:

- frame queue
- inference thread 분리
- 결과 JSON 저장
- 이벤트 기준 알림 로직

산출물:

- 데모 영상
- metrics CSV
- 이벤트 로그
- 병목 분석

## Week 8. Portfolio Packaging

목표:

- 면접에서 설명 가능한 형태로 결과를 정리한다.

실행:

```bash
python scripts/06_make_run_report.py \
  --title "Jetson Orin Nano Edge AI Camera Demo" \
  --metrics outputs/metrics.csv \
  --output reports/week8_portfolio_report.md
```

README에 포함할 내용:

- 문제 정의
- 하드웨어/소프트웨어 환경
- 데이터셋 구성
- 모델 선택 이유
- 학습 결과
- ONNX/TensorRT 변환 결과
- 실시간 데모 결과
- 실패 사례와 개선 방향

면접 답변 구조:

```text
1. 카메라 제품에서 필요한 문제를 정의했습니다.
2. Jetson Orin Nano에서 입력부터 추론까지 pipeline을 구성했습니다.
3. pretrained 모델을 baseline으로 삼고, custom data로 fine-tuning했습니다.
4. ONNX/TensorRT 변환 후 FPS와 latency를 측정했습니다.
5. false positive/false negative를 분석해 데이터와 threshold 개선 방향을 도출했습니다.
```

