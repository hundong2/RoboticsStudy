# 02. CLI와 그래프 디버깅

목표는 GUI 없이도 ROS 시스템 상태를 파악하는 것입니다. 실제 로봇에서는 화면이 없거나 원격 SSH만 가능한 경우가 많기 때문에 CLI 디버깅이 매우 중요합니다.

## 기본 관찰 순서

```bash
ros2 node list
ros2 topic list -t
ros2 topic echo /topic_name
ros2 topic hz /topic_name
ros2 topic info /topic_name -v
ros2 param list
ros2 service list -t
ros2 action list -t
```

## 문제별 접근법

| 증상 | 먼저 볼 것 | 다음 확인 |
|---|---|---|
| 노드가 안 보임 | `ros2 node list` | 환경 source, Domain ID, 네트워크 |
| topic은 있는데 데이터가 없음 | `ros2 topic echo` | publisher 실행 여부 |
| 데이터가 느림 | `ros2 topic hz` | CPU, QoS, 네트워크 |
| subscriber가 못 받음 | `ros2 topic info -v` | message type, QoS |
| RViz에 안 보임 | TF tree | frame id, timestamp |
| Nav2가 멈춤 | costmap topic | sensor, footprint, TF |

## QoS 기본

QoS는 통신 품질 설정입니다. 카메라나 LiDAR 같은 센서 데이터는 최신 값이 중요하고, 맵이나 설정 정보는 놓치면 안 되는 경우가 많습니다.

| 설정 | 의미 | 실무 감각 |
|---|---|---|
| reliability | 손실 허용 여부 | 센서는 best effort, 명령은 reliable 검토 |
| durability | 늦게 붙은 subscriber가 과거 값을 받을지 | static map, latched-like 데이터에 중요 |
| history | 몇 개를 보관할지 | queue depth와 지연의 균형 |

## rosbag2

```bash
ros2 bag record /cmd_vel /odom /tf
ros2 bag info <bag_directory>
ros2 bag play <bag_directory>
```

rosbag2는 실패를 재현하는 핵심 도구입니다. 실제 로봇에서 문제가 생겼을 때 topic을 기록해두면, 하드웨어 없이도 개발 PC에서 다시 분석할 수 있습니다.

## 실습

1. turtlesim을 실행합니다.
2. 키보드 조작 노드를 실행합니다.
3. `/turtle1/cmd_vel`을 echo합니다.
4. `ros2 topic hz /turtle1/pose`로 pose 갱신 주기를 확인합니다.
5. `/turtle1/cmd_vel`과 `/turtle1/pose`를 rosbag2로 기록합니다.
6. 기록한 bag을 재생하며 echo 결과가 재현되는지 확인합니다.

## 통과 기준

- topic 이름만 보고도 데이터 방향을 추론할 수 있다.
- message type 불일치와 QoS 불일치를 구분한다.
- rosbag2가 디버깅, 회귀 테스트, 데이터셋 수집에 쓰인다는 점을 설명한다.
