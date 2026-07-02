# 환경 구축 가이드

이 가이드는 Jetson Orin Nano Developer Kit과 카메라 모듈을 기준으로 합니다.

## 1. 준비물

- Jetson Orin Nano Developer Kit
- microSD 또는 NVMe SSD
- JetPack 6.x 권장
- CSI 카메라 또는 USB 카메라
- 모니터, 키보드, 마우스 또는 SSH 접속 환경
- 안정적인 전원 어댑터

## 2. JetPack 설치 확인

Jetson에서 터미널을 열고 확인합니다.

```bash
cat /etc/nv_tegra_release
uname -a
```

NVIDIA 도구 확인:

```bash
nvcc --version
dpkg -l | grep nvinfer
trtexec --version
```

`trtexec`가 없다면 TensorRT 설치가 제대로 되지 않은 상태입니다. JetPack을 SDK Manager 또는 공식 이미지로 다시 확인하는 편이 빠릅니다.

## 3. 시스템 패키지 설치

```bash
sudo apt update
sudo apt install -y \
  python3-pip python3-venv python3-dev \
  git cmake build-essential \
  libopenblas-dev libjpeg-dev zlib1g-dev \
  v4l-utils ffmpeg \
  gstreamer1.0-tools \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly
```

## 4. Python 가상환경

```bash
cd ~/RoboticsStudy/Research/2026-07-02_Jetson_Edge_AI_Productization

python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
pip install -r requirements-jetson.txt
```

Jetson의 PyTorch는 일반 PC처럼 단순히 `pip install torch`로 끝나지 않는 경우가 많습니다. JetPack 버전에 맞는 NVIDIA 제공 PyTorch wheel을 쓰는 것이 안전합니다.

확인:

```bash
python - <<'PY'
import torch
print(torch.__version__)
print(torch.cuda.is_available())
print(torch.cuda.get_device_name(0) if torch.cuda.is_available() else "no cuda")
PY
```

## 5. 카메라 확인

USB 카메라:

```bash
v4l2-ctl --list-devices
python scripts/01_camera_preview.py --source usb --device 0
```

CSI 카메라:

```bash
gst-launch-1.0 nvarguscamerasrc ! 'video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1' ! nvvidconv ! nveglglessink
python scripts/01_camera_preview.py --source csi
```

RTSP 카메라:

```bash
python scripts/01_camera_preview.py --source rtsp --uri rtsp://USER:PASS@IP:PORT/stream
```

## 6. Jetson 성능 모드

실험 전에는 전력 모드를 고정합니다.

```bash
sudo nvpmodel -q
sudo nvpmodel -m 0
sudo jetson_clocks
```

모니터링:

```bash
tegrastats
```

## 7. 첫 실행

```bash
python scripts/00_check_jetson.py
python scripts/02_yolo_camera_infer.py --source csi --model yolo11n.pt
```

처음 실행 시 `yolo11n.pt`가 자동 다운로드됩니다. 네트워크가 막혀 있으면 PC에서 모델을 받은 뒤 Jetson으로 복사하세요.

## 8. ONNX와 TensorRT

ONNX export:

```bash
python scripts/03_export_yolo_onnx.py --model yolo11n.pt --imgsz 640
```

TensorRT engine 생성:

```bash
trtexec \
  --onnx=yolo11n.onnx \
  --saveEngine=yolo11n_fp16.engine \
  --fp16 \
  --shapes=images:1x3x640x640
```

성능 확인:

```bash
trtexec --loadEngine=yolo11n_fp16.engine --shapes=images:1x3x640x640
```

## 9. 자주 막히는 지점

| 증상 | 원인 후보 | 해결 방향 |
| --- | --- | --- |
| CSI 카메라가 열리지 않음 | ribbon cable 방향, nvargus 문제 | 케이블 확인, `sudo systemctl restart nvargus-daemon` |
| OpenCV에서 CSI 입력 실패 | GStreamer backend 문제 | `cv2.getBuildInformation()`에서 GStreamer 확인 |
| torch CUDA가 false | JetPack과 torch wheel 불일치 | JetPack 버전에 맞는 NVIDIA wheel 설치 |
| TensorRT 변환 실패 | ONNX opset/shape 문제 | static shape로 export, opset 조정 |
| FPS가 낮음 | 전력 모드, 전처리 병목 | `jetson_clocks`, 입력 해상도 축소, TensorRT 사용 |

