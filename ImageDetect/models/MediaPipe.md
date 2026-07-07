# MediaPipe Object Detector

MediaPipe는 Google AI Edge의 크로스플랫폼 ML 파이프라인/Tasks 프레임워크입니다. Object Detector는 이미지, 비디오 프레임, 라이브 카메라 입력을 지원하고, TFLite 모델 메타데이터를 이용해 전처리와 라벨 처리를 단순화합니다.

## 추천 사용 경로

- 첫 실습 모델: `assets/models/efficientdet_lite0_int8.tflite`
- 이미지 예제: `examples/python/mediapipe_object_detector_image.py`
- 웹캠 예제: `examples/python/mediapipe_object_detector_camera.py`

## 공식 링크

- MediaPipe 저장소: https://github.com/google-ai-edge/mediapipe
- Object Detector 개요: https://developers.google.com/edge/mediapipe/solutions/vision/object_detector
- Python 가이드: https://developers.google.com/edge/mediapipe/solutions/vision/object_detector/python
- 라이선스: Apache 2.0

## 다운로드 경로

| 모델 | 입력 | 특징 | 다운로드 |
| --- | --- | --- | --- |
| EfficientDet-Lite0 int8 | 320x320 | 가장 가볍고 CPU 친화적 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/int8/latest/efficientdet_lite0.tflite |
| EfficientDet-Lite0 float16 | 320x320 | GPU/모바일에서 균형 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/float16/latest/efficientdet_lite0.tflite |
| EfficientDet-Lite0 float32 | 320x320 | 양자화 없는 기준 모델 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/float32/latest/efficientdet_lite0.tflite |
| EfficientDet-Lite2 int8 | 448x448 | Lite0보다 정확도 우선 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite2/int8/latest/efficientdet_lite2.tflite |
| EfficientDet-Lite2 float16 | 448x448 | 정확도/크기 균형 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite2/float16/latest/efficientdet_lite2.tflite |
| EfficientDet-Lite2 float32 | 448x448 | 비교 기준 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite2/float32/latest/efficientdet_lite2.tflite |
| SSD MobileNetV2 int8 | 256x256 | 빠른 경량 모델 | https://storage.googleapis.com/mediapipe-models/object_detector/ssd_mobilenet_v2/int8/latest/ssd_mobilenet_v2.tflite |
| SSD MobileNetV2 float32 | 256x256 | 빠른 기준 모델 | https://storage.googleapis.com/mediapipe-models/object_detector/ssd_mobilenet_v2/float32/latest/ssd_mobilenet_v2.tflite |

## 이미지 실습

```powershell
python .\examples\python\mediapipe_object_detector_image.py `
  --image .\assets\images\coco_000000039769.jpg `
  --model .\assets\models\efficientdet_lite0_int8.tflite `
  --score 0.25
```

결과 이미지는 기본적으로 `outputs/mediapipe_result.jpg`에 저장됩니다.

## 웹캠 실습

```powershell
python .\examples\python\mediapipe_object_detector_camera.py `
  --model .\assets\models\efficientdet_lite0_int8.tflite `
  --camera 0 `
  --score 0.35
```

`Esc` 또는 `q`를 누르면 종료됩니다.

## 실습 과제

1. `--score`를 낮추면 탐지 수가 늘고 오탐도 늘어납니다. 이미지 3장에 대해 결과를 비교합니다.
2. Lite0 int8, Lite2 int8, SSD MobileNetV2 int8 모델을 차례대로 받아 FPS를 비교합니다.
3. 웹캠에서 물체가 가까워질 때 box 크기와 confidence가 어떻게 변하는지 기록합니다.
4. 특정 클래스만 감지하도록 category allowlist 기능을 추가해 봅니다.
