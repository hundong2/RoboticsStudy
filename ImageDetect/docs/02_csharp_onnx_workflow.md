# 02. C# ONNX 객체 탐지 워크플로

C#에서 객체 탐지를 붙일 때 가장 단순한 조합은 다음입니다.

1. 모델을 ONNX로 준비합니다.
2. `OpenCvSharp4`로 이미지 또는 카메라 프레임을 읽습니다.
3. `CvDnn.ReadNetFromOnnx()`로 모델을 로드합니다.
4. 프레임을 모델 입력 크기에 맞게 resize/letterbox 처리합니다.
5. 출력 텐서를 모델별 규칙에 맞게 파싱합니다.
6. confidence threshold와 NMS로 박스를 정리합니다.
7. `Cv2.Rectangle`, `Cv2.PutText`로 결과를 표시합니다.

## YOLOX 실행 예

```powershell
cd D:\workspace\RoboticsStudy\ImageDetect
powershell -ExecutionPolicy Bypass -File .\scripts\download_models.ps1 -Model yolox_tiny

dotnet script .\examples\csharp\yolox_opencvsharp.csx -- `
  --model .\assets\models\yolox_tiny.onnx `
  --image .\assets\images\coco_000000039769.jpg `
  --classes .\assets\classes\coco.names `
  --conf 0.3 `
  --nms 0.45
```

카메라:

```powershell
dotnet script .\examples\csharp\yolox_opencvsharp.csx -- `
  --model .\assets\models\yolox_tiny.onnx `
  --camera 0 `
  --classes .\assets\classes\coco.names
```

## 다른 ONNX 모델을 붙일 때

먼저 출력 구조를 확인합니다.

```powershell
dotnet script .\examples\csharp\onnx_output_inspector.csx -- `
  --model .\assets\models\some_model.onnx `
  --image .\assets\images\coco_000000039769.jpg
```

출력 구조를 확인한 뒤, 모델 문서에 맞춰 후처리 파서를 따로 작성합니다.

- YOLOX: `[cx, cy, w, h, objectness, class scores...]`
- RT-DETR: export 방식에 따라 `logits`, `pred_boxes` 또는 후처리된 boxes/scores/labels
- EfficientDet: 보통 boxes/classes/scores/num_detections 계열

후처리는 모델마다 달라지는 부분입니다. 입력 처리와 카메라 루프는 대부분 재사용할 수 있습니다.
