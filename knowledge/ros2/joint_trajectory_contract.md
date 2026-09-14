# ROS 2 `JointTrajectory` 계약

`trajectory_msgs/msg/JointTrajectory`는 여러 관절의 시간 순서 목표를 전달하는 표준 메시지다. 값만 맞아도 되는 배열이 아니라 이름, 축 순서, 기준 시각, 누적 시간이 함께 맞아야 한다.

## 최소 불변조건

1. `joint_names`는 중복 없이 controller가 가진 joint 이름과 일치해야 한다.
2. 각 point의 `positions/velocities/accelerations/effort`가 존재한다면 길이는 `joint_names`와 같아야 한다.
3. `time_from_start`는 0 이상이며 point 순서에 따라 단조 증가해야 한다.
4. 각 수치는 finite여야 하고 joint/velocity/acceleration/effort limit 안이어야 한다.
5. `header.stamp`의 clock domain과 controller의 clock이 일치해야 한다.

## QoS 선택

- 계획 경로/정적 목표: 늦게 시작한 consumer가 필요하면 `Reliable + TransientLocal + KeepLast(1)`을 고려한다.
- 고주기 streaming setpoint: 낡은 표본보다 최신값이 중요하면 작은 depth와 best-effort를 고려한다.
- QoS는 publisher/subscriber 양쪽의 compatibility를 확인해야 한다. “더 강한 QoS”가 자동으로 더 안전한 것은 아니다.

## 제품 경계

`JointTrajectory`가 충돌 안전, 부드러움, controller 수용을 스스로 보장하지 않는다. 입력 validator, continuous collision check, time parameterization, watchdog, controller tolerance를 별도 계층으로 둔다.

참고: [ROS 2 Jazzy JointTrajectory 정의](https://docs.ros.org/en/jazzy/p/trajectory_msgs/msg/JointTrajectory.html)
