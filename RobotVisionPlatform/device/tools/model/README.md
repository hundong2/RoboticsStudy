# Model Tools

`device/guides/onnx-workflow.md`를 자동화하는 작은 CLI 도구 모음입니다.

```bash
uv sync
uv run python inspect_onnx.py model.onnx
uv run python validate_onnx.py model.onnx --shape images=1,3,640,640
uv run python create_manifest.py --help
```

| 파일 | 역할 |
|---|---|
| `inspect_onnx.py` | ONNX checker, shape inference, 입출력 계약 출력 |
| `validate_onnx.py` | CPU/CUDA/TensorRT EP 추론과 출력 통계·평균 시간 측정 |
| `create_manifest.py` | artifact hash와 전처리 계약을 포함한 manifest 생성 |
| `export_ultralytics.py` | Ultralytics checkpoint를 ONNX bundle 경로로 export |
| `build_tensorrt_engine.sh` | Jetson에서 `trtexec` FP16 engine 생성과 benchmark |

Ultralytics export 도구까지 설치하려면 `uv sync --extra export`를 사용합니다. 실제 모델과 생성된 TensorRT engine은 이 도구 폴더에 저장하지 말고 version별 model bundle 또는 외부 model registry에 저장합니다.
