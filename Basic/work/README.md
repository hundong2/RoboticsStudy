# Jetson Orin Nano / Raspberry Pi 실습 Work 폴더

이 폴더는 `Basic` 학습 코스를 실제 보드에서 따라 해보기 위한 단계별 Python 가이드입니다.

## 공식 기준 문서

- Jetson Orin Nano Dev Kit Quick Start: https://docs.nvidia.com/jetson/orin-nano-devkit/user-guide/latest/quick_start.html
- JetPack SDK 다운로드/릴리스 정보: https://developer.nvidia.com/embedded/jetpack/downloads
- Raspberry Pi Camera 문서: https://www.raspberrypi.com/documentation/accessories/camera.html
- Picamera2 manual: https://pip.raspberrypi.com/documents/RP-008156-DS-picamera2-manual.pdf
- Nav2 documentation: https://docs.nav2.org/
- Nav2 with SLAM: https://docs.nav2.org/tutorials/docs/navigation2_with_slam.html

## 폴더 구조

```text
work/
  shared/          공통 점검, 카메라, 데이터 수집, YOLO/Ollama 예제
  jetson_orin/     Jetson Orin Nano Developer Kit 기준 실행 가이드
  raspberry_pi/    Raspberry Pi + Camera Module 기준 실행 가이드
  slam_nav2/       SLAM/Nav2 실행 가이드와 점검 스크립트
```

## 추천 실습 순서

1. 공통 환경 점검: `python shared/00_check_platform.py`
2. 카메라 확인: Jetson은 `jetson_orin/01_jetson_camera_preview.py`, Raspberry Pi는 `raspberry_pi/01_picamera2_preview.py`
3. 데이터 수집: `python shared/02_capture_dataset.py --camera auto --out data/camera_samples`
4. 비전 추론: `python shared/03_yolo_camera_infer.py --source 0`
5. Ollama 명령 파싱: `python shared/04_ollama_command_parser.py "빨간 컵을 집어 왼쪽에 놓아줘"`
6. ROS2 topic 구조 이해: `python shared/05_ros2_camera_node_guide.py`
7. SLAM/Nav2 점검: `python slam_nav2/01_slam_nav2_readiness_check.py`
8. SLAM 실행 명령 생성: `python slam_nav2/02_slam_nav2_command_builder.py --ros-distro humble --mode turtlebot3`

## SLAM 실습 전제

SLAM을 제대로 하려면 보통 2D LiDAR가 `/scan` topic으로 들어와야 합니다. 카메라 모듈만 있다면 아래 중 하나로 시작하세요.

- 시뮬레이션: TurtleBot3/Gazebo + slam_toolbox + Nav2
- 실제 로봇: Jetson/Raspberry Pi + 2D LiDAR + wheel odometry + TF
- 카메라 기반: ORB-SLAM/RTAB-Map 같은 Visual SLAM. 단, Nav2의 표준 2D costmap 주행과 바로 같지는 않습니다.

## 주의

이 폴더의 파일은 실습 가이드 겸 시작 코드입니다. 실제 로봇을 움직일 때는 모터 전원, 비상정지, 충돌 공간, 속도 제한을 먼저 확인하세요.
