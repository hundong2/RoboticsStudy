# Jetson에서 TensorRT engine 만들기

## 왜 Jetson에서 만드는가?

TensorRT engine(`.engine`, `.plan`)은 TensorRT 버전, GPU compute capability, builder 설정에 영향을 받습니다. PC에서 만든 engine을 그대로 복사하기보다 범용 ONNX를 배포하고 대상과 동일한 JetPack/TensorRT 환경에서 engine을 생성하는 방식을 기본값으로 사용합니다.

## 1. 설치 확인

```bash
dpkg-query -W nvidia-jetpack
trtexec --version
```

JetPack이 설치되어도 `trtexec`가 PATH에 없으면 `/usr/src/tensorrt/bin/trtexec`를 확인합니다.

## 2. FP16 engine 생성

고정 shape 모델:

```bash
./device/tools/model/build_tensorrt_engine.sh \
  --onnx /opt/robot-vision/models/person-detector/0.1.0/model.onnx \
  --engine /opt/robot-vision/models/person-detector/0.1.0/model.fp16.engine \
  --input images \
  --shape 1x3x640x640
```

dynamic shape 모델:

```bash
./device/tools/model/build_tensorrt_engine.sh \
  --onnx model.onnx --engine model.fp16.engine --input images \
  --min-shape 1x3x320x320 \
  --opt-shape 1x3x640x640 \
  --max-shape 4x3x1280x1280
```

`opt-shape`은 가장 자주 사용할 크기로 지정합니다. 최대 범위를 불필요하게 크게 잡으면 engine 생성 시간과 메모리 사용량이 늘 수 있습니다.

## 3. benchmark 읽는 법

스크립트는 engine 생성 후 `trtexec --loadEngine` benchmark를 실행합니다. 확인할 값:

- GPU Compute Time: GPU가 추론 계산에 사용한 시간
- Host Latency: 입력 준비와 enqueue 등을 포함한 host 관점 시간
- Throughput: 초당 처리 횟수
- percentile: 평균뿐 아니라 p95/p99 지연 확인

`trtexec` 결과는 모델 단독 성능입니다. 최종 승인은 카메라 캡처, 전처리, 후처리, 인코딩을 모두 포함한 device pipeline의 end-to-end latency로 합니다.

## 4. INT8은 나중에 적용

FP16을 먼저 정확도 기준선으로 만듭니다. INT8은 실제 운영 장면을 대표하는 calibration dataset, calibration cache 관리, class별 정확도 회귀 검사가 준비된 뒤 사용합니다. 단순히 `--int8`만 추가한 결과를 production에 배포하지 않습니다.

## 5. 실패할 때

- `Unsupported operator`: exporter/opset을 확인하거나 TensorRT plugin/custom layer를 검토
- dynamic input 오류: 입력 tensor 이름과 min/opt/max profile 확인
- engine load 실패: 다른 JetPack/TensorRT/GPU에서 생성된 engine인지 확인
- 속도가 느림: CPU fallback, 입력 복사, power mode, thermal throttling을 함께 확인

공식 자료: [TensorRT ONNX deployment quick start](https://docs.nvidia.com/deeplearning/tensorrt/latest/getting-started/quick-start-onnx-deployment.html), [TensorRT dynamic shapes](https://docs.nvidia.com/deeplearning/tensorrt/latest/inference-library/work-with-dynamic-shapes.html)
