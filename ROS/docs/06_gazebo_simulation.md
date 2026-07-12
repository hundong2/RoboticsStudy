# 06. Gazebo 시뮬레이션

Gazebo는 로봇 모델, 월드, 물리 엔진, 센서 플러그인을 이용해 실제 하드웨어 없이 ROS 시스템을 검증하는 도구입니다.

## 시뮬레이션을 먼저 하는 이유

- 하드웨어 파손 위험 없이 주행 알고리즘을 테스트합니다.
- 센서 topic, TF, controller 연결을 빠르게 점검합니다.
- rosbag2와 CI를 연결해 회귀 테스트를 만들 수 있습니다.
- 실제 로봇 투입 전에 요구사항을 정량화할 수 있습니다.

## 꼭 구분해야 할 시간

| 시간 | 의미 |
|---|---|
| wall time | 실제 컴퓨터의 시간 |
| sim time | 시뮬레이터가 제공하는 가상 시간 |

시뮬레이션 노드는 보통 `use_sim_time:=true`를 사용합니다. 이 설정이 서로 다르면 TF와 sensor timestamp가 어긋날 수 있습니다.

## Gazebo와 ROS 2 연결 요소

- robot description: URDF 또는 SDF 모델입니다.
- controller: 바퀴, 조인트, 그리퍼를 제어합니다.
- sensor plugin: 카메라, LiDAR, IMU topic을 발행합니다.
- bridge: Gazebo transport와 ROS topic을 연결합니다.

## 실습

1. URDF 모델을 RViz에서 먼저 확인합니다.
2. Gazebo 월드에 모델을 spawn합니다.
3. `/clock` topic이 나오는지 확인합니다.
4. `use_sim_time` parameter가 true인지 확인합니다.
5. `/cmd_vel`을 publish해 로봇이 움직이는지 확인합니다.

## 통과 기준

- RViz는 시각화 도구, Gazebo는 물리 시뮬레이터라는 차이를 설명한다.
- `use_sim_time` 불일치가 TF 오류로 이어지는 이유를 설명한다.
- 실제 로봇 전환 시 바뀌는 부분과 유지되는 부분을 분리한다.
