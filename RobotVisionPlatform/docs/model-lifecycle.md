# 모델 수명주기

## 흐름

1. 수집: 개인정보 마스킹, scene/device metadata, 중복 제거 후 샘플만 보존
2. 라벨: 클래스 정의서와 edge-case bucket을 먼저 고정
3. 분할: 같은 영상/장소가 train과 validation에 섞이지 않도록 group split
4. 학습: pretrained detector fine-tuning, augmentation ablation, 실험 추적
5. 평가: mAP 외에 class별 precision/recall, latency, memory, thermal throttling 측정
6. export: ONNX 검증 후 target Jetson에서 TensorRT engine 생성
7. 승인: manifest와 artifact hash/signature 검증
8. 배포: canary 1대 → cohort → fleet, health/accuracy guardrail 위반 시 rollback
9. 개선: uncertainty/오탐/미탐 후보를 active-learning queue로 회수

TensorRT engine은 GPU/JetPack/TensorRT 버전에 민감할 수 있으므로 범용 ONNX와 target별 engine을 구분하고 manifest에 호환 조건을 기록합니다. 모델 파일만 교체하지 말고 labels, preprocessing, postprocessing, calibration cache를 하나의 불변 bundle로 취급합니다.

예시 manifest:

```json
{
  "modelId": "forklift-detector",
  "version": "0.1.0",
  "format": "onnx",
  "sha256": "replace-me",
  "input": { "width": 640, "height": 640, "color": "RGB" },
  "labels": ["person", "forklift"],
  "minimumRuntime": { "jetpack": "6.x", "tensorrt": "verify-on-target" }
}
```

