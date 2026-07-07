# 03. 샘플 자산

## 이미지

`assets/images`에는 COCO 2017 validation 이미지 3장이 들어 있습니다.

| 파일 | 원본 URL |
| --- | --- |
| `coco_000000000139.jpg` | `http://images.cocodataset.org/val2017/000000000139.jpg` |
| `coco_000000039769.jpg` | `http://images.cocodataset.org/val2017/000000039769.jpg` |
| `coco_000000397133.jpg` | `http://images.cocodataset.org/val2017/000000397133.jpg` |

다시 받기:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\download_sample_images.ps1
```

## 클래스 목록

`assets/classes/coco.names`는 YOLOX 같은 COCO 사전학습 ONNX 모델에서 쓰는 80개 클래스 이름입니다.

MediaPipe TFLite 모델은 보통 모델 메타데이터 안에 label map이 포함되어 있어 별도 `.names` 파일이 없어도 됩니다.

## 포함된 모델

| 파일 | 원본 URL | 용도 |
| --- | --- | --- |
| `assets/models/efficientdet_lite0_int8.tflite` | `https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/int8/latest/efficientdet_lite0.tflite` | MediaPipe 이미지/카메라 실습 |

YOLOX와 RT-DETR 모델은 용량이 커서 기본 포함하지 않았습니다. `scripts/download_models.ps1`로 필요할 때 내려받으세요.
