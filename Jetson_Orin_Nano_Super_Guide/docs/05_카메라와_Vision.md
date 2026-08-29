# 5. 카메라와 Vision 파이프라인

## 5.1 입력 방식 선택

| 입력 | 일반 경로 | 장점 | 주의점 |
|---|---|---|---|
| CSI Bayer | libargus / `nvarguscamerasrc` | Jetson ISP 사용, 낮은 복사 비용 | 센서 드라이버와 ISP 튜닝 필요 |
| CSI RAW | V4L2 direct | 드라이버 검증, RAW 캡처 | ISP 후처리 없음 |
| USB UVC | V4L2 / `v4l2src` | 연결과 호환이 비교적 쉬움 | USB 대역폭·지연·압축 형식 |
| GMSL/CoE | Argus 또는 SIPL 경로 | 장거리·다중 카메라 | serializer/deserializer, 동기화, 드라이버 통합 |

r39.2 문서는 Jetson 전체 로드맵에서 SIPL을 주요 신규 카메라 경로로 설명하지만, Orin Nano의 MIPI CSI와 기존 ISP 워크플로에서는 Argus가 여전히 핵심이다. SIPL의 모든 Thor 기능이 Orin Nano에서 동일하다고 가정하지 말고 Camera Support Matrix를 확인한다.

## 5.2 첫 카메라 검증

장치 확인:

```bash
v4l2-ctl --list-devices
v4l2-ctl --device=/dev/video0 --list-formats-ext
media-ctl -p
```

CSI Argus 미리보기 예:

```bash
gst-launch-1.0 nvarguscamerasrc sensor-id=0 ! \
  'video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1' ! \
  nvvidconv ! autovideosink
```

USB UVC 예:

```bash
gst-launch-1.0 v4l2src device=/dev/video0 ! \
  videoconvert ! autovideosink
```

실제 format은 `v4l2-ctl --list-formats-ext` 결과와 맞춘다. 헤드리스 환경에서는 display sink 대신 encoder/filesink 또는 fakesink를 사용한다.

## 5.3 카메라 드라이버 브링업

Bayer 센서는 다음을 함께 구현해야 한다.

1. I2C 센서 드라이버와 register table
2. 전원 rail, clock, reset/powerdown 시퀀스
3. V4L2 controls와 mode table
4. device tree의 module, sensor, endpoint, CSI/VI topology
5. lane 수, clock, pixel format, line length, frame length
6. Argus 사용 시 ISP tuning 및 camera core 연동

직접 V4L2 경로는 RAW 검증에 적합하다. ISP를 사용하는 제품은 libargus 또는 GStreamer `nvarguscamerasrc` 경로를 검증해야 한다. NVIDIA 공개 BSP에는 모든 센서용 ISP tuning 도구가 포함되지 않으므로 인증 카메라 파트너의 지원 범위를 확인한다.

## 5.4 Zero-copy에 가까운 파이프라인

Vision VLA의 병목은 모델만이 아니라 프레임 복사와 색공간 변환이다. 가능한 경우 다음 경로를 유지한다.

```text
CSI/USB 입력
  -> NVMM/NvBufSurface
  -> VIC 또는 CUDA 전처리
  -> TensorRT/Isaac ROS NITROS 추론
  -> 소형 구조화 결과
  -> VLM 또는 행동 정책
```

CPU OpenCV `cv::Mat`로 매 프레임 복사한 뒤 다시 GPU로 올리는 구조는 해상도와 FPS가 높을수록 손해가 커진다. GStreamer caps에서 `memory:NVMM`, Isaac ROS에서는 NITROS 협상을 확인한다.

## 5.5 VPI 사용

VPI는 CPU, CUDA, VIC, OFA 등 가속 백엔드에서 비동기 Vision 연산을 구성한다. Orin Nano는 PVA가 없는 구성일 수 있으므로 VPI 문서의 플랫폼별 backend 표를 확인한다. 전처리, remap, optical flow, stereo, feature detection을 GPU 모델과 병렬화할 때 유용하다.

VPI 설계 원칙:

- payload와 buffer를 매 프레임 생성하지 않고 재사용한다.
- stream과 event로 단계 사이를 동기화한다.
- backend 변경 전 정확도·latency·memory copy를 함께 측정한다.
- 입력 해상도별 payload 제약을 확인한다.

## 5.6 데이터셋 수집 규칙

- 카메라 원본 시간, ROS timestamp, robot state, action, instruction을 같은 episode ID로 묶는다.
- 자동 노출/화이트밸런스와 수동 고정 조건을 모두 기록한다.
- calibration 파일, lens/focus, 해상도, FPS, sensor mode를 메타데이터에 남긴다.
- 실패 행동과 안전 개입도 삭제하지 않고 별도 레이블로 보존한다.
- 사람·문서·주거 공간 촬영 시 개인정보와 동의 절차를 둔다.

## 5.7 캘리브레이션과 시간 동기화

VLA가 행동을 출력하려면 이미지와 관절 상태의 시간 정렬이 중요하다. 카메라 intrinsics, distortion, camera-to-base extrinsics를 버전 관리하고, 다중 카메라는 hardware trigger/PTP 가능 여부를 검토한다. 소프트웨어 timestamp만 사용할 때는 capture time과 message publish time을 구분해 latency budget에 포함한다.

