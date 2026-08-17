# 모델 bundle과 배포 규칙

모델 한 버전은 아래 파일을 함께 배포합니다.

```text
person-detector/0.1.0/
├── model.onnx
├── model.fp16.engine       # 대상 Jetson에서 생성, Git에는 저장하지 않음
├── labels.txt
├── manifest.json
├── preprocessing.json      # 필요하면 별도 상세 설정
└── calibration.cache       # INT8일 때만
```

## manifest가 필요한 이유

`model.onnx`만 보면 RGB/BGR, resize 방식, normalization, label 순서, threshold를 알 수 없습니다. 서로 다른 설정으로 같은 모델 파일을 실행하면 조용히 잘못된 결과가 생길 수 있습니다. manifest는 실행 코드가 시작 전에 이 조건을 검증할 수 있게 합니다.

예제: `device/models/example/manifest.json`

배포 과정:

1. 서버가 manifest와 서명을 제공합니다.
2. 장치는 임시 version 폴더에 파일을 받습니다.
3. 각 SHA-256과 호환 조건을 검사합니다.
4. ONNX를 검사하고 TensorRT engine을 생성하거나 검증합니다.
5. smoke test를 통과하면 `current` symlink를 새 version으로 atomic switch합니다.
6. health guardrail 위반 시 이전 symlink로 rollback합니다.

engine은 Git에 commit하지 않습니다. ONNX도 크기가 크면 Git LFS나 모델 registry/object storage에서 관리하고 저장소에는 manifest만 둡니다.

