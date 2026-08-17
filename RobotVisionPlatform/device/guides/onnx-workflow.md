# ONNX 실전 흐름

## ONNX가 하는 일

ONNX는 PyTorch 같은 학습 도구에서 만든 모델을 다른 추론 도구로 옮기기 위한 공통 모델 형식입니다. 이 프로젝트에서는 다음 세 가지 목적으로 사용합니다.

1. 학습 코드와 Jetson 실행 코드를 분리합니다.
2. 모델의 입력 이름·크기·자료형과 출력 구조를 자동 검사합니다.
3. ONNX Runtime 결과를 TensorRT 결과와 비교하는 기준으로 사용합니다.

ONNX 파일만으로는 전처리, label 순서, confidence threshold, NMS 방식이 모두 표현되지 않을 수 있습니다. 따라서 `model.onnx`, `labels.txt`, `manifest.json`을 항상 하나의 bundle로 관리합니다.

## 1. Python 환경 준비

PC에서 실행합니다. 프로젝트가 사용하는 패키지만 격리하기 위해 `uv`를 권장합니다.

```bash
cd device/tools/model
uv sync
```

일반 Python 환경이라면 다음도 가능합니다.

```bash
python -m venv .venv
# Windows: .venv\Scripts\activate
# Linux: source .venv/bin/activate
python -m pip install -e .
```

## 2. 학습 모델 export

Ultralytics YOLO 계열 예시:

```bash
uv sync --extra export
python export_ultralytics.py \
  --weights runs/detect/train/weights/best.pt \
  --output artifacts/person-detector/model.onnx \
  --image-size 640 \
  --opset 17
```

`ultralytics`는 export할 때만 필요한 선택 dependency라서 기본 `uv sync`에는 설치되지 않습니다.

`--dynamic`은 입력 크기나 batch가 실제로 바뀌어야 할 때만 사용합니다. 고정된 `1x3x640x640` 입력은 TensorRT engine 생성과 메모리 예측이 더 단순합니다.

export 옵션은 모델 라이브러리와 TensorRT 버전에 따라 달라집니다. opset 숫자를 무조건 최신으로 올리기보다 대상 Jetson의 TensorRT parser에서 읽히는지 확인합니다.

## 3. ONNX 구조 검사

```bash
python inspect_onnx.py artifacts/person-detector/model.onnx \
  --save-inferred artifacts/person-detector/model.inferred.onnx
```

이 명령은 ONNX checker를 실행하고 입력·출력 이름, shape, type, opset을 출력합니다. `-1`, `?`, 문자로 표시되는 차원은 실행 시 결정되는 dynamic dimension입니다.

## 4. ONNX Runtime 추론 검증

```bash
python validate_onnx.py artifacts/person-detector/model.onnx \
  --provider cpu \
  --shape images=1,3,640,640 \
  --runs 10
```

출력 shape, 최소/최대/평균, warm-up 이후 평균 시간이 표시됩니다. 무작위 입력은 실행 가능 여부만 검사합니다. 정확도를 검증하려면 학습 전처리를 거친 `.npy` 입력과 기대 출력을 별도 회귀 테스트로 만들어야 합니다.

TensorRT Execution Provider가 설치된 Jetson/서버에서는 다음처럼 확인할 수 있습니다.

```bash
python validate_onnx.py model.onnx --provider tensorrt --shape images=1,3,640,640
```

provider 우선순위는 TensorRT → CUDA → CPU입니다. 일부 node가 fallback되면 성능이 예상보다 낮을 수 있으므로 verbose log나 profile로 실제 할당을 확인합니다.

## 5. manifest 생성

```bash
python create_manifest.py \
  --model model.onnx \
  --model-id person-detector \
  --version 0.1.0 \
  --labels labels.txt \
  --input-name images \
  --input-shape 1,3,640,640 \
  --color RGB \
  --output manifest.json
```

manifest의 SHA-256은 서버와 장치가 다운로드 파일이 손상되거나 바뀌지 않았는지 확인할 때 사용합니다. 보안 배포에서는 checksum과 별도로 manifest 전자서명이 필요합니다.

## 공식 참고

- [ONNX 개요](https://onnx.ai/onnx/intro/)
- [ONNX Runtime Execution Providers](https://onnxruntime.ai/docs/execution-providers/)
- [ONNX Runtime TensorRT EP](https://onnxruntime.ai/docs/execution-providers/TensorRT-ExecutionProvider.html)
- [TensorRT ONNX opset guide](https://docs.nvidia.com/deeplearning/tensorrt/latest/reference/onnx-opset-guide.html)
