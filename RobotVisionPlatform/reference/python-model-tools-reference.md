# Python 모델 도구 사전

## `argparse.ArgumentParser`

CLI 옵션을 선언하고 자동 도움말과 타입 변환을 제공합니다.

```python
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("model", type=Path)
parser.add_argument("--runs", type=int, default=10)
args = parser.parse_args()
```

사용자는 `python tool.py --help`로 옵션을 확인합니다. 잘못된 필수 옵션은 실행 전에 오류가 됩니다.

## `pathlib.Path`

운영체제와 무관하게 파일 경로를 다룹니다.

```python
model = Path("models/model.onnx")
model.exists()
model.parent.mkdir(parents=True, exist_ok=True)
data = model.read_bytes()
```

문자열 경로 결합보다 Windows와 Linux separator 차이를 줄여 줍니다.

## type hint

```python
def sha256(path: Path) -> str:
```

`path`는 Path, 반환값은 str이라는 설명입니다. Python runtime이 자동 강제하지 않으므로 editor와 type checker의 도움을 받습니다.

## docstring

함수 첫 줄의 문자열은 함수 사용 목적을 설명합니다.

```python
def parse_shapes(items: list[str]) -> dict[str, tuple[int, ...]]:
    """Parse repeated shape options into concrete dimensions."""
```

`help(parse_shapes)`나 IDE hover에서 볼 수 있습니다.

## `hashlib.sha256`

파일 내용의 고정 길이 fingerprint를 계산합니다.

```python
digest = hashlib.sha256()
with path.open("rb") as source:
    for chunk in iter(lambda: source.read(1024 * 1024), b""):
        digest.update(chunk)
checksum = digest.hexdigest()
```

큰 ONNX 파일을 한 번에 RAM에 올리지 않고 1 MiB씩 읽습니다. checksum은 손상 확인용이며 배포 신뢰성에는 별도 전자서명이 필요합니다.

## `json.dumps`

Python dictionary를 JSON 문자열로 변환합니다.

```python
text = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
```

- `ensure_ascii=False`: 한글을 읽을 수 있게 유지합니다.
- `indent=2`: 사람이 검토하기 쉬운 formatting입니다.

## `onnx.load`

```python
model = onnx.load(path, load_external_data=True)
```

ONNX graph와 weight를 읽습니다. `load_external_data=True`는 큰 weight가 외부 파일로 분리된 모델도 함께 읽습니다.

## `onnx.checker.check_model`

graph 연결, operator schema, type 등 ONNX 구조가 유효한지 확인합니다.

```python
checker.check_model(model)
```

통과는 정확도가 좋다는 뜻이 아니라 “ONNX 구조가 규칙에 맞다”는 뜻입니다.

## `shape_inference.infer_shapes`

입력과 operator 정보로 계산 가능한 중간/output shape를 채웁니다.

```python
inferred = shape_inference.infer_shapes(model)
```

custom operator나 data-dependent shape는 모두 추론되지 않을 수 있습니다.

## `onnxruntime.InferenceSession`

ONNX graph를 지정 execution provider에서 실행할 session으로 준비합니다.

```python
session = ort.InferenceSession(
    "model.onnx",
    providers=["CUDAExecutionProvider", "CPUExecutionProvider"],
)
```

provider는 앞에서부터 우선합니다. 프로젝트 도구는 요청한 GPU provider가 없을 때 CPU로 조용히 대체하지 않고 오류를 냅니다.

## `session.get_inputs()` / `get_outputs()`

모델의 runtime tensor 계약을 읽습니다.

```python
for item in session.get_inputs():
    print(item.name, item.shape, item.type)
```

manifest의 input 이름·shape·type과 비교할 때 사용합니다.

## `session.run`

```python
outputs = session.run(None, {"images": input_array})
```

- 첫 인자 `None`: 모든 output을 반환합니다.
- dictionary key: 정확한 ONNX input tensor 이름입니다.
- value: shape/type이 맞는 NumPy array입니다.

첫 실행에는 graph 최적화와 cache 준비 시간이 포함될 수 있어 benchmark 전에 warm-up합니다.

## NumPy `default_rng`, `astype`, `isfinite`

```python
rng = np.random.default_rng(42)
input_array = rng.random(shape).astype(np.float32)
finite = output[np.isfinite(output)]
```

- 고정 seed는 같은 random input을 재현합니다.
- `astype`은 ONNX input type에 맞춥니다.
- `isfinite`는 NaN/Inf를 통계에서 분리합니다.

random input은 실행 가능성 검사에만 적합합니다. 모델 정확도 비교에는 실제 전처리를 적용한 고정 `.npy` tensor를 사용합니다.

## `subprocess.run(check=True)`

테스트가 CLI를 별도 process로 실행합니다.

```python
subprocess.run([sys.executable, str(tool), "--help"], check=True)
```

`check=True`이면 command 실패가 exception이 되어 test도 실패합니다. 문자열 한 줄보다 argument list가 shell quoting에 안전합니다.

## `tempfile.TemporaryDirectory`

테스트 전용 파일을 만들고 scope가 끝나면 자동 정리합니다.

```python
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
```

실제 model directory를 오염시키지 않고 file I/O를 검증할 수 있습니다.

