# URDF, JointState, robot_state_publisher

## 역할 분리

- URDF `link`: 질량·관성·시각·충돌 형상을 가진 강체
- URDF `joint`: parent-child link 사이의 원점, 축, 운동 종류와 제한
- `sensor_msgs/msg/JointState`: 가동 joint의 현재 위치·속도·힘
- `robot_state_publisher`: URDF와 JointState를 결합해 TF를 발행

가동 관절은 보통 `/tf`, 고정 관절은 `/tf_static`으로 발행된다. `JointState.name`은 URDF joint 이름과 정확히 같아야 하며, `name[i]`, `position[i]`, `velocity[i]`, `effort[i]`는 같은 관절을 나타낸다.

## origin을 읽는 법

joint의 `<origin xyz="..." rpy="..."/>`는 parent link 좌표계에서 child joint 좌표계로 가는 고정 변환이다. revolute joint의 현재 회전은 그 뒤 `<axis>` 방향으로 적용된다. visual의 origin은 TF가 아니라 link 좌표계 안에서 형상을 어디에 그릴지 정한다.

## 점검 목록

1. 구조가 단일 parent를 갖는 트리인가?
2. 길이는 m, 각은 rad인가(ROS REP-103)?
3. 회전축과 오른손 법칙이 의도와 맞는가?
4. joint limit가 실제 하드웨어보다 느슨하지 않은가?
5. `check_urdf`, RViz, `tf2_echo`로 구조와 운동을 확인했는가?

## 참고

- [ROS 2 Jazzy URDF tutorial](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/URDF/Using-URDF-with-Robot-State-Publisher.html)
- [robot_state_publisher](https://docs.ros.org/en/jazzy/p/robot_state_publisher/)
- [REP-103](https://www.ros.org/reps/rep-0103.html)
