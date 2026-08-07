# 03. 로봇 없는 환경에서 시뮬레이션 테스트

실제 로봇 없이도 [closed-loop](../glossary/README.md#closed-loop) 테스트를 설계할 수 있습니다. 핵심은 "모델 입력과 출력이 실제 ROS 그래프를 타고 환경을 바꾸는가?"입니다.

## 단계별 전략

| 단계 | 목표 | 도구 |
|---:|---|---|
| 1 | C++ 노드가 topic을 읽고 쓰는지 확인 | fake publisher/subscriber |
| 2 | 센서 bag 재생으로 open-loop 출력 확인 | [rosbag2](../glossary/README.md#rosbag2) |
| 3 | 시뮬레이터에서 closed-loop 확인 | [Gazebo](../glossary/README.md#gazebo), [Isaac Sim](../glossary/README.md#isaac-sim) |
| 4 | 물리/제어 알고리즘 실험 | [PyBullet](../glossary/README.md#pybullet), [MuJoCo](../glossary/README.md#mujoco) |
| 5 | launch test와 CI 연결 | launch_testing |

## Gazebo 기반 흐름

```text
Gazebo world
  -> /scan, /odom, /tf
  -> ml_policy_node
  -> /policy/cmd_vel_raw
  -> safety_filter
  -> /cmd_vel
  -> Gazebo robot
```

[ros_gz](../glossary/README.md#ros-gz)는 Gazebo Transport와 ROS 2 topic을 연결합니다. Gazebo에서 나온 센서가 ROS 2로 들어오고, ROS 2의 `/cmd_vel`이 Gazebo 모델을 움직이면 closed-loop가 됩니다.

## Isaac Sim 기반 흐름

[Isaac Sim](../glossary/README.md#isaac-sim)은 ROS 2 Bridge로 `/clock`, `/tf`, camera, LiDAR, joint state를 연결할 수 있습니다. 고품질 camera, synthetic data, GPU 기반 perception 검증이 필요할 때 적합합니다.

## PyBullet/MuJoCo 활용 위치

[PyBullet](../glossary/README.md#pybullet)과 [MuJoCo](../glossary/README.md#mujoco)는 ROS 2 전체 시스템보다 동역학, 제어, 강화학습 알고리즘 실험에 강합니다. 이 경우 bridge를 직접 설계합니다.

```text
physics simulator state
  -> ROS topic style observation
  -> policy
  -> simulator command
  -> next state
```

## 무로봇 테스트 체크리스트

- [ ] 시뮬레이터가 `/clock`을 publish한다.
- [ ] 모든 노드가 `use_sim_time:=true`를 사용한다.
- [ ] 센서 메시지의 `header.frame_id`가 [TF](../glossary/README.md#tf) 트리에 존재한다.
- [ ] C++ 정책 노드 출력이 `/policy/cmd_vel_raw`로 분리되어 있다.
- [ ] 안전 필터가 최종 `/cmd_vel`을 publish한다.
- [ ] 실패한 run을 [rosbag2](../glossary/README.md#rosbag2)로 저장한다.
- [ ] 같은 bag으로 모델 버전 A/B 출력을 비교한다.

## 평가 지표

| 지표 | 의미 |
|---|---|
| success rate | 목표를 달성한 비율 |
| collision rate | 충돌한 비율 |
| timeout rate | 시간 안에 실패한 비율 |
| intervention count | 사람이 개입한 횟수 |
| p99 latency | 최악에 가까운 추론 지연 |
| command saturation | 속도 제한에 걸린 비율 |

## 통과 기준

- open-loop bag replay와 closed-loop simulation의 차이를 설명한다.
- Gazebo/Isaac/PyBullet/MuJoCo 중 어떤 도구를 왜 선택하는지 말한다.
- 실제 로봇 없이도 실패 재현과 모델 회귀 테스트를 설계한다.
