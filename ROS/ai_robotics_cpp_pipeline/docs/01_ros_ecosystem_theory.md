# 01. ROS 생태계 이론

ROS 생태계는 단순히 라이브러리 모음이 아닙니다. [노드](../glossary/README.md#node), [토픽](../glossary/README.md#topic), [서비스](../glossary/README.md#service), [액션](../glossary/README.md#action), [TF](../glossary/README.md#tf), [rosbag2](../glossary/README.md#rosbag2), [ros2_control](../glossary/README.md#ros2-control), [시뮬레이터](../glossary/README.md#simulator), 배포 도구가 연결된 로봇 소프트웨어 운영 체계입니다.

## 큰 그림

```text
센서 드라이버
  -> perception / ML inference
  -> planning / AI agent / behavior tree
  -> safety filter
  -> controller
  -> hardware interface 또는 simulator
```

## 계층별 역할

| 계층 | 대표 기술 | 실무 질문 |
|---|---|---|
| 통신 | [topic](../glossary/README.md#topic), [service](../glossary/README.md#service), [action](../glossary/README.md#action), [QoS](../glossary/README.md#qos) | 어떤 데이터가 얼마나 자주 흐르는가? |
| 좌표계 | [TF](../glossary/README.md#tf), URDF | sensor frame과 robot frame이 맞는가? |
| 제어 | [ros2_control](../glossary/README.md#ros2-control), controller manager | 시뮬레이터와 실제 하드웨어 인터페이스가 같은가? |
| 관측 | [rosbag2](../glossary/README.md#rosbag2), log, diagnostics | 실패를 재현할 수 있는가? |
| 시뮬레이션 | [Gazebo](../glossary/README.md#gazebo), [Isaac Sim](../glossary/README.md#isaac-sim), [PyBullet](../glossary/README.md#pybullet), [MuJoCo](../glossary/README.md#mujoco) | 실제 로봇 없이 무엇을 검증할 수 있는가? |
| AI | [PyTorch](../glossary/README.md#pytorch), [ONNX Runtime](../glossary/README.md#onnx-runtime), [TensorRT](../glossary/README.md#tensorrt), [LLM Agent](../glossary/README.md#llm-agent) | 모델 출력이 제어 명령이 되어도 안전한가? |

## 초보자가 먼저 익혀야 할 관찰 순서

```bash
ros2 node list
ros2 topic list -t
ros2 topic echo /scan
ros2 topic echo /odom
ros2 topic hz /cmd_vel
ros2 service list -t
ros2 action list -t
ros2 param list
```

이 명령은 "코드를 읽기 전에 시스템을 관찰하는 눈"을 만듭니다. 로봇 실무는 실행 중인 그래프를 읽는 능력이 절반입니다.

## ML 추론 노드가 ROS 그래프 안에 들어가는 위치

```text
/camera/image_raw or /scan
  -> ml_policy_node
  -> /policy/cmd_vel_raw
  -> safety_filter_node
  -> /cmd_vel
  -> controller or simulator
```

[머신러닝 추론](../glossary/README.md#inference) 노드는 보통 센서 데이터를 받아 action을 냅니다. 그러나 추론 노드가 바로 `/cmd_vel`을 내보내면 위험합니다. 실무에서는 `raw action -> safety filter -> final command` 구조가 더 안전합니다.

## 이론 체크리스트

- [ ] [Node](../glossary/README.md#node)는 책임 단위이고 process 단위일 수 있음을 설명한다.
- [ ] [Topic](../glossary/README.md#topic)은 지속 데이터, [Service](../glossary/README.md#service)는 짧은 요청/응답, [Action](../glossary/README.md#action)은 긴 목표 수행임을 구분한다.
- [ ] [TF](../glossary/README.md#tf)가 틀리면 perception과 navigation이 모두 흔들린다는 점을 설명한다.
- [ ] [rosbag2](../glossary/README.md#rosbag2)가 실패 재현과 데이터셋 수집의 공통 도구임을 이해한다.
- [ ] [ros2_control](../glossary/README.md#ros2-control)이 시뮬레이터와 실제 하드웨어 전환을 깔끔하게 만드는 이유를 설명한다.
