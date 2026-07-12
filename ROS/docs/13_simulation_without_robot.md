# 13. 로봇 없는 환경에서 시뮬레이션 실습

실제 로봇이 없어도 ROS 2 학습은 충분히 시작할 수 있습니다. 오히려 초반에는 시뮬레이션으로 topic, TF, sensor, controller, Nav2, perception 흐름을 반복해서 익히는 편이 안전하고 빠릅니다.

기준일: 2026-07-12

## 추천 시작 순서

| 순서 | 목적 | 추천 도구 | 이유 |
|---|---|---|---|
| 1 | ROS 통신 감각 | turtlesim | 설치가 가볍고 topic/service/action 확인이 쉬움 |
| 2 | 모바일 로봇 기본 | Gazebo Sim + TurtleBot3 | ROS 2/Nav2 자료가 가장 풍부함 |
| 3 | GUI 친화적 로봇 실습 | Webots + webots_ros2 | 예제 로봇과 월드가 많고 초보자가 보기 쉬움 |
| 4 | 고품질 센서/AI/합성데이터 | Isaac Sim + ROS 2 Bridge | RTX 센서, synthetic data, Physical AI 실험에 유리 |
| 5 | 동역학/RL/제어 | MuJoCo, PyBullet, Drake | 빠른 물리 시뮬레이션과 제어/학습 연구에 유리 |
| 6 | 자율주행 차량 | CARLA + ROS bridge | 차량, 도로, LiDAR/Radar/카메라 시나리오 특화 |

## 무료 또는 무료로 시작 가능한 프레임워크

| 프레임워크 | 비용/라이선스 관점 | ROS 2 연계 | 가장 좋은 용도 | 주의점 |
|---|---|---|---|---|
| Gazebo Sim | 오픈소스 | `ros_gz` bridge | 범용 ROS 2 로봇 시뮬레이션, Nav2, 센서 | ROS 배포판과 Gazebo 버전 호환 확인 |
| Webots | 오픈소스 3D 로봇 시뮬레이터 | `webots_ros2` | 초보자 GUI 실습, 교육용 로봇, 센서 확인 | 예제별 ROS 배포판 지원 확인 |
| NVIDIA Isaac Sim | 무료 사용 가능, Apache 2.0 구성과 추가 라이선스 조건 확인 필요 | ROS 2 Bridge | 고품질 렌더링, 합성데이터, Isaac ROS, GPU 기반 AI | NVIDIA GPU/드라이버 요구, 재배포 조건 확인 |
| MuJoCo | 무료 오픈소스 물리 엔진 | 커뮤니티 ROS 2 패키지, ros2_control 연계 | 정밀 동역학, 조작, 강화학습 | ROS 통합은 Gazebo보다 직접 구성할 일이 많음 |
| PyBullet/Bullet | 오픈소스 물리 SDK | 직접 bridge 작성 또는 커뮤니티 예제 | Python 기반 알고리즘/RL 빠른 실험 | ROS 표준 스택과 바로 연결되지는 않음 |
| Drake | 오픈소스 모델 기반 설계/검증 도구 | ROS와 병행 가능 | 최적제어, 동역학, 검증, 고급 제어 | 초보자용 ROS bringup 도구는 아님 |
| CoppeliaSim Edu | 학생/교사/대학 교수 무료 교육용, 상업/기관 사용 제한 | ROS 2 Interface | 교육용 다중 로봇, 빠른 GUI 실험 | Edu 라이선스 사용 범위 제한 |
| CARLA | 오픈소스 자율주행 시뮬레이터 | ROS/ROS 2 bridge | 차량 자율주행, 교통, 카메라/LiDAR/Radar | 일반 모바일 로봇보다 차량 도메인 특화 |

## ROS 학습자에게 가장 현실적인 선택

### 1순위: Gazebo Sim

Gazebo는 ROS 생태계와 가장 직접적으로 연결됩니다. URDF/SDF, 센서 plugin, `/cmd_vel`, `/odom`, `/scan`, `/tf`, Nav2 흐름을 한 번에 연습하기 좋습니다.

추천 실습:

```bash
source /opt/ros/lyrical/setup.bash
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py
ros2 topic list -t
ros2 topic echo /scan
ros2 topic echo /odom
```

확인할 것:

- `/clock`이 있는가?
- `use_sim_time`이 true인가?
- `/scan`의 `header.frame_id`가 TF 트리에 있는가?
- `/cmd_vel`을 보내면 `/odom`이 변하는가?

### 2순위: Webots

Webots는 GUI와 예제 로봇이 좋아 초보자가 "로봇이 실제로 움직인다"는 감각을 얻기 쉽습니다. `webots_ros2`는 ROS 2 message, service, action과 연결됩니다.

추천 실습:

```bash
sudo apt install ros-${ROS_DISTRO}-webots-ros2
ros2 launch webots_ros2_turtlebot robot_launch.py
ros2 topic list -t
```

확인할 것:

- Webots 월드가 열리는가?
- ROS 2 topic으로 센서 데이터가 나오는가?
- RViz에서 TF와 sensor frame이 맞는가?

### 3순위: Isaac Sim

Isaac Sim은 GPU 기반 고품질 시뮬레이션, synthetic data, Isaac ROS, 로봇 AI 실험에 강합니다. 단, 초보자가 첫 ROS 그래프를 배우는 용도로는 무겁습니다.

추천 실습:

1. Isaac Sim 설치와 GPU 드라이버 요구사항을 확인합니다.
2. ROS 2 Bridge를 활성화합니다.
3. 기본 ROS interface로 `/clock`, `/tf`, camera topic을 publish합니다.
4. ROS 2 쪽에서 `ros2 topic list -t`와 RViz로 확인합니다.

### 4순위: MuJoCo/PyBullet/Drake

이 도구들은 "ROS 전체 시스템"보다 "동역학, 제어, 학습 알고리즘" 실험에 강합니다.

추천 사용:

- MuJoCo: 로봇 팔, 보행, 접촉 동역학, 강화학습
- PyBullet: Python으로 빠른 알고리즘 실험
- Drake: 최적제어, 궤적 최적화, 모델 기반 검증

ROS와 연결할 때는 다음 경계를 명확히 합니다.

```text
simulation state -> /joint_states, /odom, /tf
ROS command      -> /cmd_vel, trajectory action, controller command
```

### 5순위: CARLA

CARLA는 자율주행 차량 도메인에 특화되어 있습니다. 일반 실내 서비스 로봇보다 차량 센서, 도로, traffic agent, semantic segmentation 학습에 적합합니다.

추천 실습:

```bash
ros2 launch carla_ros_bridge carla_ros_bridge.launch.py
ros2 topic list -t
```

## 로봇 없는 환경 실습 루트

| 주차 | 실습 | 완료 기준 |
|---|---|---|
| 1 | turtlesim topic/service/action | `/turtle1/cmd_vel`과 `/turtle1/pose` 설명 |
| 2 | Gazebo TurtleBot3 수동 주행 | `/cmd_vel -> /odom` 흐름 설명 |
| 3 | Gazebo 또는 Webots 센서 확인 | `/scan`, image topic, TF frame 확인 |
| 4 | SLAM 지도 생성 | map 저장과 `use_sim_time` 설명 |
| 5 | Nav2 목표 이동 | global/local costmap과 실패 원인 분석 |
| 6 | rosbag2 회귀 실습 | 같은 bag으로 오류를 재현 |
| 7 | Isaac Sim 또는 Webots perception | camera topic과 detection output 설계 |
| 8 | MuJoCo/PyBullet/Drake 선택 실습 | 동역학/제어 실험 결과 기록 |

## 실습 프로젝트

구체적인 명령과 미션은 [projects/06_robotless_simulation_lab](../projects/06_robotless_simulation_lab/README.md)에 정리했습니다.

## 공식 자료

- Gazebo ROS 2 설치/연동: <https://gazebosim.org/docs/latest/ros_installation/>
- Gazebo ROS 2 integration: <https://gazebosim.org/docs/latest/ros2_integration/>
- Webots ROS 2 패키지: <https://github.com/cyberbotics/webots_ros2>
- Isaac Sim ROS 2 Bridge: <https://docs.isaacsim.omniverse.nvidia.com/6.0.0/installation/install_ros.html>
- Isaac Sim 라이선스: <https://docs.isaacsim.omniverse.nvidia.com/5.0.0/common/licenses-isaac-sim.html>
- MuJoCo: <https://mujoco.org/>
- Bullet/PyBullet: <https://github.com/bulletphysics/bullet3>
- Drake: <https://drake.mit.edu/>
- CoppeliaSim ROS 2 Interface: <https://manual.coppeliarobotics.com/en/ros2Interface.htm>
- CoppeliaSim Edu 조건: <https://www.coppeliarobotics.com/>
- CARLA ROS bridge: <https://carla.readthedocs.io/projects/ros-bridge/en/latest/run_ros/>
