# ImageDetect

상업 프로젝트에서 소스 코드를 비공개로 유지하기 쉬운 Apache 2.0 계열 객체 탐지 모델을 비교하고, 이미지 파일과 웹캠으로 직접 실습하는 통합 커리큘럼입니다.

> 법률 자문은 아니지만, Apache 2.0/MIT 계열은 보통 상업적 사용과 비공개 소스 배포가 가능합니다. 배포 시에는 원 저작권/라이선스 고지와 NOTICE 파일 요구 사항을 확인하세요. 사전학습 가중치와 학습 데이터의 조건은 모델 코드 라이선스와 별도로 확인하는 습관을 들이는 것이 좋습니다.

## 폴더 구성

```text
ImageDetect/
  README.md
  docs/
    00_environment.md
    01_model_file_formats.md
    02_csharp_onnx_workflow.md
    03_sample_assets.md
  models/
    YOLOX.md
    RT-DETR.md
    EfficientDet.md
    MediaPipe.md
  examples/
    csharp/
      camera_preview.csx
      yolox_opencvsharp.csx
      onnx_output_inspector.csx
    python/
      mediapipe_object_detector_image.py
      mediapipe_object_detector_camera.py
      rtdetr_huggingface_image.py
  assets/
    classes/coco.names
    images/
    models/efficientdet_lite0_int8.tflite
  scripts/
    download_sample_images.ps1
    download_models.ps1
```

## 빠른 시작

### 1. MediaPipe 이미지/카메라 실습

```powershell
cd D:\workspace\RoboticsStudy\ImageDetect
py -3.11 -m venv .venv
.\.venv\Scripts\activate
pip install -r requirements.txt

python .\examples\python\mediapipe_object_detector_image.py `
  --image .\assets\images\coco_000000039769.jpg `
  --model .\assets\models\efficientdet_lite0_int8.tflite

python .\examples\python\mediapipe_object_detector_camera.py `
  --model .\assets\models\efficientdet_lite0_int8.tflite `
  --camera 0
```

### 2. C# + OpenCVSharp + YOLOX ONNX 실습

```powershell
dotnet tool install -g dotnet-script
powershell -ExecutionPolicy Bypass -File .\scripts\download_models.ps1 -Model yolox_tiny

dotnet script .\examples\csharp\yolox_opencvsharp.csx -- `
  --model .\assets\models\yolox_tiny.onnx `
  --image .\assets\images\coco_000000039769.jpg `
  --classes .\assets\classes\coco.names

dotnet script .\examples\csharp\yolox_opencvsharp.csx -- `
  --model .\assets\models\yolox_tiny.onnx `
  --camera 0 `
  --classes .\assets\classes\coco.names
```

YOLOX ONNX 파일은 20MB 이상입니다. 저장소에는 포함하지 않았고 `download_models.ps1`로 필요할 때 내려받습니다.

## 모델 선택 요약

| 모델 | 라이선스 | 추천 포맷 | 실습 난이도 | 좋은 용도 |
| --- | --- | --- | --- | --- |
| YOLOX | Apache 2.0 | ONNX | 중 | C#/OpenCVSharp, 범용 객체 탐지, YOLO 대체 |
| RT-DETR / RT-DETRv2 | Apache 2.0 | PyTorch, ONNX export | 중상 | YOLO 계열과 다른 Transformer 기반 실시간 탐지 |
| EfficientDet | Apache 2.0 | TF checkpoint, TFLite, ONNX 변환 | 중 | 모바일/엣지, 경량-정확도 균형 비교 |
| MediaPipe Object Detector | Apache 2.0 | TFLite task model | 하 | 빠른 프로토타입, 카메라 실습, 라즈베리파이/엣지 |

## 모델 파일 확장자 요약

객체 탐지 모델은 보통 "구조", "가중치", "클래스 이름", "배포용 변환 파일"이 나뉘어 있습니다. 최신 프레임워크는 이 정보를 하나의 파일에 묶기도 하고, Darknet처럼 구조와 가중치를 따로 두기도 합니다.

| 확장자 | 주 사용처 | 의미 | 비고 |
| --- | --- | --- | --- |
| `.cfg` | Darknet YOLO | 네트워크 구조를 정의한 텍스트 설정 파일 | 레이어, 입력 크기, anchor, 클래스 수 등이 들어 있습니다. |
| `.weights` | Darknet YOLO | 학습된 가중치 바이너리 파일 | 저장소에 항상 포함되는 것은 아니고, 용량 때문에 별도 링크로 받는 경우가 많습니다. |
| `.conv.*` | Darknet YOLO | backbone 사전학습 가중치 | 탐지 모델 전체가 아니라 학습 시작용 가중치인 경우가 많습니다. |
| `.backup` | Darknet YOLO | 학습 중/학습 후 저장된 체크포인트 | 직접 학습하면 `backup/` 폴더에 생길 수 있습니다. 이름만 다르고 역할은 가중치에 가깝습니다. |
| `.names`, `.txt` | 공통 | 클래스 이름 목록 | COCO 80개 클래스는 `assets/classes/coco.names`에 있습니다. |
| `.pt`, `.pth` | PyTorch | 모델 가중치 또는 체크포인트 | YOLOX, RT-DETR PyTorch 모델에서 자주 씁니다. |
| `.pdparams` | PaddlePaddle | Paddle 모델 가중치 | PaddleDetection의 RT-DETR에서 사용합니다. |
| `.h5` | TensorFlow/Keras | Keras 모델 또는 가중치 | EfficientDet 원본 체크포인트에서 볼 수 있습니다. |
| `.ckpt`, `.tar.gz`, `.tgz` | TensorFlow | 체크포인트 묶음 | 여러 파일을 압축해서 배포하는 경우가 많습니다. |
| `.onnx` | ONNX Runtime, OpenCV DNN | 여러 언어/런타임에서 쓰기 위한 범용 추론 그래프 | C#에서는 가장 다루기 쉬운 포맷입니다. |
| `.tflite` | TensorFlow Lite, MediaPipe | 모바일/엣지 추론용 모델 | MediaPipe Object Detector 실습에서 사용합니다. |
| `.engine`, `.trt` | TensorRT | NVIDIA GPU 최적화 엔진 | 특정 GPU/드라이버 환경에 묶이는 경우가 많습니다. |
| `.xml` + `.bin` | OpenVINO | Intel OpenVINO 모델 구조와 가중치 | 변환된 모델은 두 파일이 쌍으로 필요합니다. |

Darknet 모델에서 `.weights` 파일이 안 보이는 경우가 있습니다. 보통은 저장소가 `.cfg`만 제공하고 가중치는 README의 다운로드 링크로 따로 받게 되어 있거나, 학습 전이라 아직 `.weights`/`.backup`이 생성되지 않은 상태입니다. 또는 누군가 Darknet 모델을 이미 `.onnx`, `.pt`, `.tflite` 같은 배포 포맷으로 변환해 둔 경우 원본 `.weights` 없이도 실행할 수 있습니다.

## 통합 커리큘럼

| 차시 | 주제 | 산출물 |
| --- | --- | --- |
| 1 | 라이선스와 모델 파일 형식 | `.onnx`, `.tflite`, `.pth`, `.pdparams` 차이 설명 |
| 2 | 이미지/카메라 입력 다루기 | `camera_preview.csx`로 웹캠 프레임 확인 |
| 3 | MediaPipe Object Detector | 이미지 결과 파일과 카메라 실시간 탐지 |
| 4 | YOLOX ONNX + C# | COCO 이미지 탐지, 웹캠 탐지, confidence/NMS 조절 |
| 5 | RT-DETR/RT-DETRv2 | 공식 가중치 다운로드, ONNX export, 출력 구조 확인 |
| 6 | EfficientDet | Google AutoML 체크포인트와 TFLite 모델 비교 |
| 7 | 성능 비교 | FPS, latency, 입력 크기, confidence threshold 표 만들기 |
| 8 | 로봇 응용 | 카메라 좌표계, 탐지 박스 중심점, 로봇 제어 이벤트 설계 |

## 학습 순서

1. [환경 준비](docs/00_environment.md)를 먼저 확인합니다.
2. [모델 파일 형식](docs/01_model_file_formats.md)을 읽고 포맷별 실행 경로를 구분합니다.
3. [샘플 자산](docs/03_sample_assets.md)으로 이미지와 모델을 확인합니다.
4. 초반 실습은 [MediaPipe](models/MediaPipe.md)로 시작합니다.
5. C# 배포 흐름은 [YOLOX](models/YOLOX.md)와 [C# ONNX 워크플로](docs/02_csharp_onnx_workflow.md)를 따라갑니다.
6. 고급 비교는 [RT-DETR](models/RT-DETR.md), [EfficientDet](models/EfficientDet.md)를 확장 실습으로 진행합니다.

## 공식 자료

- YOLOX: https://github.com/Megvii-BaseDetection/YOLOX
- RT-DETR: https://github.com/lyuwenyu/RT-DETR
- EfficientDet: https://github.com/google/automl/tree/master/efficientdet
- MediaPipe Object Detector: https://developers.google.com/edge/mediapipe/solutions/vision/object_detector
- COCO dataset: https://cocodataset.org/
