# 06. 로봇 없는 환경 시뮬레이션 실습

이 프로젝트는 실제 로봇 없이 ROS 2 시스템을 연습하기 위한 실습 묶음입니다. 처음에는 Gazebo 또는 Webots를 사용하고, 필요에 따라 Isaac Sim, MuJoCo, PyBullet, Drake, CARLA로 확장합니다.

## 실습 A: Gazebo + TurtleBot3

목표: 실제 모바일 로봇 없이 `/cmd_vel`, `/odom`, `/scan`, `/tf`, Nav2 흐름을 익힙니다.

설치 예시:

```bash
source /opt/ros/lyrical/setup.bash
sudo apt update
sudo apt install ros-${ROS_DISTRO}-turtlebot3*
sudo apt install ros-${ROS_DISTRO}-nav2-bringup
```

실행 예시:

```bash
export TURTLEBOT3_MODEL=burger
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py
```

관찰:

```bash
ros2 topic list -t
ros2 topic echo /scan
ros2 topic echo /odom
ros2 run tf2_tools view_frames
```

통과 기준:

- `/cmd_vel`이 로봇 속도 명령 topic이라는 점을 설명합니다.
- `/odom`이 로봇 이동 추정값이라는 점을 설명합니다.
- `/scan`의 frame id가 TF 트리에 있어야 하는 이유를 설명합니다.

## 실습 B: Gazebo + SLAM + Nav2

목표: 맵 생성과 목표 지점 이동을 시뮬레이션에서 먼저 연습합니다.

SLAM:

```bash
ros2 launch slam_toolbox online_async_launch.py use_sim_time:=true
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```

맵 저장:

```bash
ros2 run nav2_map_server map_saver_cli -f my_first_map
```

Navigation:

```bash
ros2 launch nav2_bringup localization_launch.py map:=my_first_map.yaml use_sim_time:=true
ros2 launch nav2_bringup navigation_launch.py use_sim_time:=true
```

통과 기준:

- `use_sim_time`이 false일 때 어떤 문제가 생길 수 있는지 설명합니다.
- costmap에 장애물이 표시되지 않을 때 `/scan`, TF, parameter 중 무엇을 볼지 순서대로 말합니다.

## 실습 C: Webots + ROS 2

목표: GUI 친화적인 로봇 월드에서 ROS 2 topic과 sensor를 확인합니다.

설치 예시:

```bash
source /opt/ros/lyrical/setup.bash
sudo apt install ros-${ROS_DISTRO}-webots-ros2
```

실행 예시:

```bash
ros2 launch webots_ros2_turtlebot robot_launch.py
```

관찰:

```bash
ros2 node list
ros2 topic list -t
ros2 topic echo /tf
```

통과 기준:

- Webots 시뮬레이터와 ROS 2 노드가 어떤 topic으로 연결되는지 설명합니다.
- Gazebo와 Webots 중 어떤 상황에서 무엇을 선택할지 말합니다.

## 실습 D: Isaac Sim + ROS 2 Bridge

목표: GPU 기반 고품질 센서 시뮬레이션과 ROS 2 Bridge 개념을 이해합니다.

권장 조건:

- NVIDIA GPU
- Isaac Sim 지원 드라이버
- Ubuntu 24.04 + ROS 2 Jazzy 또는 문서상 지원 조합

실습 절차:

1. Isaac Sim 설치 문서에서 지원 OS/ROS 조합을 확인합니다.
2. ROS 2 Bridge를 활성화합니다.
3. 기본 scene에서 `/clock`, `/tf`, camera topic이 publish되는지 확인합니다.
4. RViz에서 image, TF, robot model을 확인합니다.

통과 기준:

- Isaac Sim이 언제 Gazebo/Webots보다 적합한지 설명합니다.
- synthetic data, RTX sensor, GPU acceleration이 perception 학습에 주는 장점을 설명합니다.

## 실습 E: MuJoCo/PyBullet/Drake 선택 실습

목표: ROS 전체 시스템이 아니라 동역학, 제어, 강화학습 관점에서 시뮬레이터를 선택합니다.

선택 기준:

| 목적 | 추천 |
|---|---|
| 빠른 Python 강화학습 실험 | PyBullet |
| 접촉/동역학 정확도와 RL | MuJoCo |
| 최적제어와 검증 | Drake |

ROS 연결 미션:

1. 시뮬레이터의 로봇 상태를 `/joint_states` 또는 `/odom` 형태로 생각해 봅니다.
2. ROS 명령을 `/cmd_vel` 또는 trajectory command로 생각해 봅니다.
3. 어떤 정보를 TF로 publish해야 하는지 적습니다.

통과 기준:

- "시뮬레이터 내부 상태"와 "ROS topic으로 공개할 상태"를 분리해서 설명합니다.
- 물리 엔진 선택이 학습 속도, 정확도, ROS 통합 난이도에 미치는 영향을 설명합니다.

## 실습 F: CARLA + ROS bridge

목표: 자율주행 차량 도메인에서 ROS 2 bridge 구조를 이해합니다.

실행 예시:

```bash
ros2 launch carla_ros_bridge carla_ros_bridge.launch.py
ros2 topic list -t
```

통과 기준:

- CARLA가 일반 모바일 로봇보다 차량/도로/교통 시나리오에 특화되어 있다는 점을 설명합니다.
- camera, LiDAR, Radar, vehicle control topic을 구분합니다.

## 프레임워크 선택 결론

- 처음 배우는 ROS 2 학습자는 Gazebo 또는 Webots로 시작합니다.
- Nav2를 목표로 하면 Gazebo가 가장 무난합니다.
- GUI와 교육용 예제를 우선하면 Webots가 편합니다.
- AI perception, synthetic data, Isaac ROS를 다루려면 Isaac Sim을 검토합니다.
- 강화학습/동역학 연구는 MuJoCo/PyBullet/Drake를 별도 실험 축으로 둡니다.
- 자율주행 차량이면 CARLA를 선택합니다.
