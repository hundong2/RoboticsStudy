# 01. turtlesim CLI 실습

목표는 코드를 작성하기 전에 ROS 그래프를 눈과 손으로 익히는 것입니다.

## 실행

터미널 1:

```bash
source /opt/ros/lyrical/setup.bash
ros2 run turtlesim turtlesim_node
```

터미널 2:

```bash
source /opt/ros/lyrical/setup.bash
ros2 run turtlesim turtle_teleop_key
```

## 관찰

터미널 3:

```bash
ros2 node list
ros2 topic list -t
ros2 topic echo /turtle1/cmd_vel
ros2 topic echo /turtle1/pose
ros2 service list -t
ros2 action list -t
```

## 미션

1. 키보드로 거북이를 움직입니다.
2. `/turtle1/cmd_vel`에서 어떤 값이 바뀌는지 확인합니다.
3. `/turtle1/pose`에서 위치와 방향이 어떻게 변하는지 확인합니다.
4. `ros2 service call /clear std_srvs/srv/Empty`로 화면을 지웁니다.
5. `ros2 bag record /turtle1/cmd_vel /turtle1/pose`로 움직임을 기록합니다.

## 제출물

- 실행한 명령어 목록
- `ros2 topic list -t` 결과
- Topic, Message, Node 관계를 그린 간단한 다이어그램
- "topic echo는 왜 디버깅에 중요한가?"에 대한 답변 3문장
