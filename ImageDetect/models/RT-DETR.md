# RT-DETR / RT-DETRv2

RT-DETR은 Baidu/PaddleDetection 계열에서 공개된 Real-Time Detection Transformer입니다. YOLO 계열처럼 빠른 실시간 탐지를 목표로 하지만, NMS 의존을 줄인 end-to-end Transformer 탐지기라는 점이 다릅니다.

## 추천 사용 경로

- 빠른 실습: Hugging Face Transformers 예제
- 원본 연구 재현: 공식 `lyuwenyu/RT-DETR` PyTorch 또는 Paddle 경로
- C#/OpenCVSharp 배포: 공식 export로 ONNX 생성 후 별도 후처리 파서 작성

## 공식 링크

- 저장소: https://github.com/lyuwenyu/RT-DETR
- Hugging Face 문서: https://huggingface.co/docs/transformers/en/model_doc/rt_detr
- 논문: https://arxiv.org/abs/2304.08069
- 라이선스: Apache 2.0

## RT-DETRv2 PyTorch 다운로드 경로

| 모델 | 체크포인트 |
| --- | --- |
| RT-DETRv2-S | https://github.com/lyuwenyu/storage/releases/download/v0.2/rtdetrv2_r18vd_120e_coco_rerun_48.1.pth |
| RT-DETRv2-M | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetrv2_r34vd_120e_coco_ema.pth |
| RT-DETRv2-M R50-m | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetrv2_r50vd_m_7x_coco_ema.pth |
| RT-DETRv2-L | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetrv2_r50vd_6x_coco_ema.pth |
| RT-DETRv2-X | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetrv2_r101vd_6x_coco_from_paddle.pth |

RT-DETRv2-S만 내려받기:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\download_models.ps1 -Model rtdetrv2_s
```

## RT-DETR PyTorch 다운로드 경로

| 모델 | 체크포인트 |
| --- | --- |
| RT-DETR-R18 | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetr_r18vd_dec3_6x_coco_from_paddle.pth |
| RT-DETR-R34 | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetr_r34vd_dec4_6x_coco_from_paddle.pth |
| RT-DETR-R50-m | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetr_r50vd_m_6x_coco_from_paddle.pth |
| RT-DETR-R50 | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetr_r50vd_6x_coco_from_paddle.pth |
| RT-DETR-R101 | https://github.com/lyuwenyu/storage/releases/download/v0.1/rtdetr_r101vd_6x_coco_from_paddle.pth |

## PaddleDetection 다운로드 경로

| 모델 | Paddle 가중치 |
| --- | --- |
| RT-DETR-R18 | https://bj.bcebos.com/v1/paddledet/models/rtdetr_r18vd_dec3_6x_coco.pdparams |
| RT-DETR-R34 | https://bj.bcebos.com/v1/paddledet/models/rtdetr_r34vd_dec4_6x_coco.pdparams |
| RT-DETR-R50-m | https://bj.bcebos.com/v1/paddledet/models/rtdetr_r50vd_m_6x_coco.pdparams |
| RT-DETR-R50 | https://bj.bcebos.com/v1/paddledet/models/rtdetr_r50vd_6x_coco.pdparams |
| RT-DETR-R101 | https://bj.bcebos.com/v1/paddledet/models/rtdetr_r101vd_6x_coco.pdparams |
| RT-DETR-L HGNetv2 | https://bj.bcebos.com/v1/paddledet/models/rtdetr_hgnetv2_l_6x_coco.pdparams |
| RT-DETR-X HGNetv2 | https://bj.bcebos.com/v1/paddledet/models/rtdetr_hgnetv2_x_6x_coco.pdparams |

## Hugging Face 이미지 실습

```powershell
pip install -r requirements-rtdetr.txt

python .\examples\python\rtdetr_huggingface_image.py `
  --image .\assets\images\coco_000000039769.jpg `
  --model-id PekingU/rtdetr_r50vd `
  --output .\outputs\rtdetr_cat_result.jpg
```

이 예제는 Hugging Face가 후처리까지 제공하므로 RT-DETR의 동작을 가장 빨리 확인할 수 있습니다.

## 공식 PyTorch ONNX export 흐름

공식 저장소를 별도 작업 폴더에 clone한 뒤 실행합니다.

```powershell
git clone https://github.com/lyuwenyu/RT-DETR.git
cd RT-DETR\rtdetrv2_pytorch
pip install -r requirements.txt

python tools\export_onnx.py `
  -c configs\rtdetrv2\rtdetrv2_r18vd_120e_coco.yml `
  -r path\to\rtdetrv2_r18vd_120e_coco_rerun_48.1.pth `
  --check
```

생성된 ONNX는 먼저 출력 텐서를 확인합니다.

```powershell
cd D:\workspace\RoboticsStudy\ImageDetect
dotnet script .\examples\csharp\onnx_output_inspector.csx -- `
  --model path\to\model.onnx `
  --image .\assets\images\coco_000000039769.jpg
```

## 실습 과제

1. Hugging Face 예제로 고양이 이미지의 `cat`, `remote`, `couch` 탐지 결과를 확인합니다.
2. `PekingU/rtdetr_r18vd`, `PekingU/rtdetr_r50vd` 모델의 속도 차이를 비교합니다.
3. 공식 PyTorch 체크포인트를 ONNX로 export한 뒤 출력 텐서 shape를 기록합니다.
4. YOLOX와 달리 RT-DETR은 NMS가 필수 후처리가 아니라는 점이 추론 파이프라인에 어떤 차이를 만드는지 정리합니다.
