# EfficientDet

EfficientDet은 Google AutoML 팀의 객체 탐지 모델입니다. EfficientNet backbone과 BiFPN 구조를 사용하며, 작은 모델부터 큰 모델까지 compound scaling으로 확장됩니다. 저장소 라이선스는 Apache 2.0입니다.

## 추천 사용 경로

- 빠른 실습: MediaPipe의 EfficientDet-Lite TFLite 모델
- 원본 연구/학습: Google AutoML EfficientDet 체크포인트
- C# 배포: TFLite보다는 ONNX 변환 후 ONNX Runtime 또는 OpenCV DNN 경로 검토

## 공식 링크

- 저장소: https://github.com/google/automl/tree/master/efficientdet
- 논문: https://arxiv.org/abs/1911.09070
- 라이선스: Apache 2.0

## Google AutoML 체크포인트 다운로드

| 모델 | Keras h5 | TensorFlow ckpt |
| --- | --- | --- |
| EfficientDet-D0 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d0.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d0.tar.gz |
| EfficientDet-D1 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d1.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d1.tar.gz |
| EfficientDet-D2 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d2.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d2.tar.gz |
| EfficientDet-D3 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d3.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d3.tar.gz |
| EfficientDet-D4 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d4.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d4.tar.gz |
| EfficientDet-D5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d5.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d5.tar.gz |
| EfficientDet-D6 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d6.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d6.tar.gz |
| EfficientDet-D7 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d7.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d7.tar.gz |
| EfficientDet-D7x | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d7x.h5 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco2/efficientdet-d7x.tar.gz |

## EfficientDet-Lite 체크포인트

| 모델 | 체크포인트 |
| --- | --- |
| EfficientDet-Lite0 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco/efficientdet-lite0.tgz |
| EfficientDet-Lite1 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco/efficientdet-lite1.tgz |
| EfficientDet-Lite2 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco/efficientdet-lite2.tgz |
| EfficientDet-Lite3 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco/efficientdet-lite3.tgz |
| EfficientDet-Lite3x | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco/efficientdet-lite3x.tgz |
| EfficientDet-Lite4 | https://storage.googleapis.com/cloud-tpu-checkpoints/efficientdet/coco/efficientdet-lite4.tgz |

## MediaPipe TFLite 실습 모델

| 모델 | TFLite |
| --- | --- |
| EfficientDet-Lite0 int8 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/int8/latest/efficientdet_lite0.tflite |
| EfficientDet-Lite0 float16 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/float16/latest/efficientdet_lite0.tflite |
| EfficientDet-Lite0 float32 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/float32/latest/efficientdet_lite0.tflite |
| EfficientDet-Lite2 int8 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite2/int8/latest/efficientdet_lite2.tflite |
| EfficientDet-Lite2 float16 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite2/float16/latest/efficientdet_lite2.tflite |
| EfficientDet-Lite2 float32 | https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite2/float32/latest/efficientdet_lite2.tflite |

이 커리큘럼에는 `assets/models/efficientdet_lite0_int8.tflite`가 포함되어 있습니다.

## 이미지 실습

```powershell
python .\examples\python\mediapipe_object_detector_image.py `
  --image .\assets\images\coco_000000397133.jpg `
  --model .\assets\models\efficientdet_lite0_int8.tflite `
  --output .\outputs\efficientdet_kitchen_result.jpg
```

## 웹캠 실습

```powershell
python .\examples\python\mediapipe_object_detector_camera.py `
  --model .\assets\models\efficientdet_lite0_int8.tflite `
  --camera 0
```

## 원본 체크포인트에서 TFLite export

공식 저장소 기준 예시입니다.

```powershell
git clone https://github.com/google/automl.git
cd automl\efficientdet

python model_inspect.py `
  --runmode=saved_model `
  --model_name=efficientdet-d0 `
  --ckpt_path=path\to\efficientdet-d0 `
  --saved_model_dir=.\saved_model_d0 `
  --tflite_path=.\efficientdet-d0.tflite
```

## 실습 과제

1. EfficientDet-Lite0 int8와 float32 모델의 파일 크기와 FPS를 비교합니다.
2. 작은 물체가 많은 이미지에서 Lite0와 Lite2 결과 차이를 비교합니다.
3. 로봇 엣지 장비에서는 정확도보다 지연 시간이 더 중요할 때가 있습니다. 어떤 threshold와 모델 크기가 적절한지 표로 정리합니다.
