# YOLOX

YOLOX는 Megvii가 공개한 anchor-free YOLO 계열 객체 탐지 모델입니다. 저장소 라이선스는 Apache 2.0이고, 공식 GitHub Release에 ONNX 파일이 함께 제공되어 C# 실습 모델로 가장 다루기 쉽습니다.

## 추천 사용 경로

- 빠른 CPU 실습: `yolox_tiny.onnx`
- 조금 더 나은 품질: `yolox_s.onnx`
- C# 실행: `examples/csharp/yolox_opencvsharp.csx`
- 클래스 파일: `assets/classes/coco.names`

## 공식 링크

- 저장소: https://github.com/Megvii-BaseDetection/YOLOX
- 릴리스: https://github.com/Megvii-BaseDetection/YOLOX/releases
- 문서: https://yolox.readthedocs.io/
- 라이선스: Apache 2.0

## 다운로드 경로

| 모델 | ONNX | PyTorch |
| --- | --- | --- |
| YOLOX-Nano | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_nano.onnx | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_nano.pth |
| YOLOX-Tiny | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_tiny.onnx | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_tiny.pth |
| YOLOX-S | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_s.onnx | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_s.pth |
| YOLOX-M | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_m.onnx | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_m.pth |
| YOLOX-L | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_l.onnx | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_l.pth |
| YOLOX-X | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_x.onnx | https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_x.pth |

스크립트로 받기:

```powershell
cd D:\workspace\RoboticsStudy\ImageDetect
powershell -ExecutionPolicy Bypass -File .\scripts\download_models.ps1 -Model yolox_tiny
powershell -ExecutionPolicy Bypass -File .\scripts\download_models.ps1 -Model yolox_s
```

## 이미지 실습

```powershell
dotnet script .\examples\csharp\yolox_opencvsharp.csx -- `
  --model .\assets\models\yolox_tiny.onnx `
  --image .\assets\images\coco_000000039769.jpg `
  --classes .\assets\classes\coco.names `
  --conf 0.3 `
  --nms 0.45 `
  --output .\outputs\yolox_cat_result.jpg
```

## 웹캠 실습

```powershell
dotnet script .\examples\csharp\yolox_opencvsharp.csx -- `
  --model .\assets\models\yolox_tiny.onnx `
  --camera 0 `
  --classes .\assets\classes\coco.names `
  --conf 0.35
```

`Esc` 또는 `q`를 누르면 종료됩니다.

## 실습 과제

1. `--conf` 값을 `0.25`, `0.5`, `0.7`로 바꿔 탐지 박스 수 변화를 비교합니다.
2. `yolox_tiny.onnx`와 `yolox_s.onnx`의 FPS와 정확도 차이를 기록합니다.
3. 웹캠에서 사람, 컵, 휴대폰처럼 가까운 물체와 멀리 있는 물체의 confidence 변화를 비교합니다.
4. 탐지 박스의 중심점 `(x + w/2, y + h/2)`을 계산해 로봇 제어 이벤트로 연결하는 의사코드를 작성합니다.

## 주의점

YOLOX ONNX 출력 파서는 YOLOX 계열 출력 구조에 맞춰져 있습니다. RT-DETR, EfficientDet ONNX를 같은 스크립트에 넣으면 출력 구조가 달라서 정상 동작하지 않습니다.
