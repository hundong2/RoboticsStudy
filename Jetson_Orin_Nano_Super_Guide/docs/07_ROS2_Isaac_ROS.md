# 7. ROS 2와 Isaac ROS

## 7.1 기준 스택

JetPack 7.2.1은 Ubuntu 24.04 계열과 ROS 2 Jazzy 조합을 기준으로 잡는다. 최신 Isaac ROS 문서는 Jetson Orin과 Jetson Thor에서 JetPack 7.2, ROS 2 Jazzy, 128GB 이상 NVMe를 지원 기준으로 제시한다. 실제 Orin Nano 8GB에서는 패키지별 메모리 요구량을 확인하고 필요한 구성만 설치한다.

ROS 2 설치는 ROS 공식 Ubuntu 24.04/Jazzy 절차를 따른다. 저장소 키와 패키지 설치 명령은 시간이 지나면 바뀔 수 있으므로 가이드북의 복사본보다 공식 설치 페이지를 우선한다.

설치 확인:

```bash
source /opt/ros/jazzy/setup.bash
ros2 doctor --report
ros2 run demo_nodes_cpp talker
```

다른 터미널:

```bash
source /opt/ros/jazzy/setup.bash
ros2 run demo_nodes_py listener
```

## 7.2 Isaac ROS 개발 환경

최신 Isaac ROS는 `isaac-ros-cli` 관리 환경을 권장한다. Docker 격리는 재현성이 높고, virtual environment는 호스트 장치 접근이 단순하며, bare metal은 고급 사용자용이다.

기본 준비:

```bash
sudo apt-get update
sudo apt-get install -y git-lfs python3-colcon-common-extensions python3-rosdep
git lfs install --skip-repo
```

Docker 모드의 개념적 순서:

1. Jetson용 Docker/NVIDIA runtime을 정상화한다.
2. `isaac-ros-cli`를 설치한다.
3. `sudo isaac-ros init docker`로 환경을 초기화한다.
4. 센서 패키지와 프로젝트 의존성을 계층형 설정으로 추가한다.
5. quickstart rosbag으로 먼저 검증한 뒤 실제 카메라로 바꾼다.

Isaac ROS는 특정 CUDA, TensorRT, OpenCV 버전을 고정할 수 있다. JetPack 기본 OpenCV와 충돌할 때 시스템 패키지를 즉시 제거하기 전에 Docker 격리를 우선 사용한다.

## 7.3 NITROS

NITROS는 ROS 2 노드 사이의 GPU 친화적 형식 협상과 type adaptation을 통해 불필요한 CPU 복사를 줄인다. 카메라에서 DNN까지 NITROS가 유지되는지 로그와 negotiated format을 확인한다. 중간에 일반 `sensor_msgs/Image` 변환이나 Python 노드가 끼면 host copy가 다시 생길 수 있다.

## 7.4 권장 Vision 구성 요소

- image pipeline: rectify, resize, format conversion
- TensorRT inference: detector, segmenter, keypoint, depth
- visual SLAM: 위치와 자세 추정
- nvblox: depth 기반 3D 재구성/장애물 지도
- AprilTag: 기준 마커와 캘리브레이션
- FoundationPose/pose estimation: 물체 자세
- cuMotion/MoveIt 연동: 조작 경로 생성

모든 패키지를 한 번에 설치하지 말고 VLA 목표에 필요한 최소 그래프부터 구성한다.

## 7.5 QoS와 실시간성

- 카메라: SensorDataQoS, 작은 depth, best-effort를 우선 검토한다.
- 상태/명령: 신뢰성이 중요하지만 오래된 명령을 쌓지 않도록 queue depth와 deadline을 설계한다.
- 정적 calibration: transient-local을 사용할 수 있다.
- 제어 명령: timestamp, sequence, expiry를 포함한다.
- watchdog: deadline miss, process heartbeat, actuator acknowledgement를 감시한다.

## 7.6 TF와 좌표계

최소 프레임:

```text
map -> odom -> base_link -> camera_link -> camera_optical_frame
                              -> arm_base -> tool0
```

이미지 좌표, optical frame, robot base, tool frame을 혼용하면 올바른 인식도 잘못된 행동으로 변환된다. URDF와 calibration 결과를 단일 저장소에서 관리하고, TF tree를 매 배포마다 검사한다.

```bash
ros2 run tf2_tools view_frames
ros2 run tf2_ros tf2_echo base_link camera_optical_frame
```

## 7.7 시뮬레이션과 HIL

- SIL: x86_64에서 Isaac Sim과 ROS 2 애플리케이션을 함께 시험한다.
- HIL: Isaac Sim은 x86_64에서 실행하고 Jetson은 실제 대상 하드웨어에서 perception/policy를 실행한다.
- 실제 로봇 전에는 recorded rosbag replay로 deterministic regression을 만든다.

Isaac Sim을 Orin Nano에서 직접 실행하는 구성은 목표로 삼지 않는다. 시뮬레이션은 고성능 x86 GPU에서, 배포는 Jetson에서 수행하는 역할 분리가 현실적이다.

