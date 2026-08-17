# 스크립트·프로토콜 사전

## PowerShell `$ErrorActionPreference = 'Stop'`

native가 아닌 PowerShell command 오류도 즉시 script 실패로 처리합니다.

```powershell
$ErrorActionPreference = 'Stop'
```

실패한 build 이후 다음 배포 명령이 계속 실행되는 것을 막습니다.

## PowerShell `$PSScriptRoot`

현재 script 파일이 있는 폴더입니다.

```powershell
$root = Split-Path -Parent $PSScriptRoot
```

사용자가 어느 폴더에서 script를 실행해도 프로젝트 경로를 찾을 수 있습니다.

## `Invoke-WebRequest`

```powershell
$response = Invoke-WebRequest -Uri $url -Method Post -ContentType 'application/json' -Body $body
```

HTTP 요청을 보내고 status/body/header를 반환합니다. [`send-demo-event.ps1`](../scripts/send-demo-event.ps1)은 수집 API smoke test에 사용합니다.

## Bash `set -euo pipefail`

```bash
set -euo pipefail
```

- `-e`: command 실패 시 종료
- `-u`: 정의되지 않은 변수 사용 시 종료
- `pipefail`: pipeline 중간 command 실패도 전체 실패

## Bash `[[ ... ]]`와 `case`

```bash
while [[ $# -gt 0 ]]; do
  case "$1" in
    --onnx) onnx_path="$2"; shift 2 ;;
    *) exit 2 ;;
  esac
done
```

CLI option을 순서대로 읽습니다. 변수는 공백이 포함된 경로를 보호하기 위해 항상 `"$value"`로 인용합니다.

## CMake `cmake -S`, `-B`, `--build`

```bash
cmake -S device -B device/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build device/build --parallel
```

- `-S`: source의 `CMakeLists.txt` 위치
- `-B`: 생성 파일과 binary가 들어갈 build 폴더
- `-G Ninja`: 사용할 build backend
- source tree와 build 결과를 분리합니다.

## CTest

```bash
ctest --test-dir device/build --output-on-failure
```

CMake의 `add_test`로 등록된 실행 파일을 수행합니다. `--output-on-failure`는 성공 로그는 줄이고 실패 원인을 보여 줍니다.

## `trtexec`

TensorRT에 포함된 model parser, engine builder, benchmark CLI입니다.

```bash
trtexec --onnx=model.onnx --saveEngine=model.engine --fp16 --shapes=images:1x3x640x640
trtexec --loadEngine=model.engine --shapes=images:1x3x640x640
```

engine은 대상 Jetson에서 생성합니다. 자세한 설명은 [TensorRT 가이드](../device/guides/tensorrt-on-jetson.md)를 봅니다.

## protobuf `message`

```proto
message Detection {
  string label = 1;
  float confidence = 2;
}
```

- 각 field 번호는 wire format의 identity입니다.
- 이미 배포한 번호를 다른 의미로 재사용하면 안 됩니다.
- 이름 변경보다 번호/타입 호환성이 더 중요합니다.

## protobuf `oneof`

```proto
oneof payload {
  Heartbeat heartbeat = 10;
  DetectionBatch detections = 11;
}
```

한 envelope에 여러 종류 중 하나만 들어가게 합니다. 새 payload는 기존 번호를 건드리지 않고 새 번호로 추가합니다.

## gRPC bidirectional streaming

```proto
rpc Connect(stream DeviceEnvelope) returns (stream ServerCommand);
```

- request 앞 `stream`: 장치가 여러 메시지를 계속 전송
- response 앞 `stream`: 서버도 여러 명령을 계속 반환
- 장치가 연결을 시작하므로 방화벽/NAT 환경에서 관리하기 쉽습니다.
- 영상 binary를 이 stream에 계속 넣기보다 WebRTC media plane과 분리합니다.

## `google.protobuf.Timestamp`

언어별 문자열 timestamp 대신 protobuf 표준 UTC timestamp를 사용합니다. `captured_at`은 카메라 촬영 시각, `sent_at`은 envelope 전송 시각이므로 둘의 차이로 장치 내부 지연을 추정할 수 있습니다.

## GitHub Actions job/step

```yaml
jobs:
  device:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - run: cmake --build device/build
```

- job은 독립 runner에서 실행되는 작업 묶음입니다.
- step은 job 안에서 순서대로 실행됩니다.
- 로컬 test script와 CI command를 같은 형태로 유지하면 환경 차이를 줄일 수 있습니다.
