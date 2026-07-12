# 01. ROS 핵심 개념

ROS는 일반적인 의미의 운영체제가 아닙니다. 로봇 애플리케이션을 여러 프로세스로 나누고, 그 프로세스들이 메시지를 주고받도록 도와주는 미들웨어와 도구 모음입니다.

## 핵심 용어

| 용어 | 한 줄 정의 | 예시 |
|---|---|---|
| Node | 하나의 역할을 맡은 실행 프로세스 | 카메라 노드, 주행 제어 노드 |
| Topic | 계속 흐르는 데이터 통로 | `/camera/image`, `/cmd_vel` |
| Message | Topic으로 오가는 데이터 형식 | `sensor_msgs/Image` |
| Service | 요청 1번, 응답 1번 통신 | 맵 저장, 모드 변경 |
| Action | 오래 걸리는 목표 수행 통신 | 목적지까지 이동 |
| Parameter | 노드 설정값 | 속도 제한, 프레임 이름 |
| TF2 | 좌표계 관계를 시간과 함께 관리 | `map -> odom -> base_link` |
| Launch | 여러 노드를 한 번에 실행 | 로봇 전체 bringup |

## Topic, Service, Action 선택 기준

| 상황 | 선택 | 이유 |
|---|---|---|
| 센서 데이터가 계속 들어온다 | Topic | 반복 스트림에 적합 |
| 한 번 요청하고 바로 결과를 받는다 | Service | 동기식 질의에 적합 |
| 오래 걸리고 중간 진행률이 필요하다 | Action | goal, feedback, result 구조 제공 |

## ROS 그래프 사고법

로봇 시스템을 볼 때는 코드를 먼저 보지 말고 데이터 흐름을 먼저 봅니다.

1. 어떤 노드가 실행 중인가?
2. 각 노드는 어떤 topic을 publish하는가?
3. 각 노드는 어떤 topic을 subscribe하는가?
4. 요청/응답이 필요한 service는 무엇인가?
5. 긴 작업을 수행하는 action은 무엇인가?
6. 좌표계 이름은 일관적인가?

## 필수 명령어

```bash
ros2 node list
ros2 topic list
ros2 service list
ros2 action list
ros2 param list
ros2 interface show geometry_msgs/msg/Twist
```

## 실습 과제

1. `turtlesim`을 실행합니다.
2. `ros2 node list`로 노드 이름을 확인합니다.
3. `ros2 topic list`로 `/turtle1/cmd_vel`을 찾습니다.
4. `ros2 interface show geometry_msgs/msg/Twist`로 속도 명령 구조를 확인합니다.
5. Topic과 Message가 왜 분리되어 있는지 설명합니다.

## 흔한 오해

- ROS는 실시간 운영체제가 아닙니다. 실시간성이 필요하면 executor, DDS, OS kernel, controller loop를 별도로 설계해야 합니다.
- ROS topic 이름만 맞는다고 시스템이 맞는 것은 아닙니다. message type, frame id, QoS, timestamp가 모두 맞아야 합니다.
- 시뮬레이션에서 되는 것이 실제 로봇에서 바로 된다는 뜻은 아닙니다. 센서 노이즈, 지연, 미끄러짐, 안전 제한이 추가됩니다.
