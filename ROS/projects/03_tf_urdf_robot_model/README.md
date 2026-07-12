# 03. TF/URDF 차동 구동 로봇 모델

목표는 로봇 모델을 link와 joint의 트리로 이해하고, 센서 frame이 TF 트리에 포함되어야 하는 이유를 익히는 것입니다.

## 포함 파일

- [urdf/mini_diffbot.urdf.xacro](urdf/mini_diffbot.urdf.xacro): 학습용 차동 구동 로봇 모델

## 학습 순서

1. `base_link`, `left_wheel_link`, `right_wheel_link`, `laser_link`, `camera_link`가 각각 무엇을 의미하는지 읽습니다.
2. 각 joint의 parent/child 관계를 손으로 그립니다.
3. RViz에서 `robot_state_publisher`로 모델을 띄우는 launch를 직접 작성합니다.
4. `/tf_static`에 고정 센서 좌표가 올라오는지 확인합니다.

## 실행 예시

```bash
ros2 launch robot_state_publisher rsp.launch.py
```

이 프로젝트에는 의도적으로 완성 launch를 넣지 않았습니다. 학습자는 [02_python_nodes_ws](../02_python_nodes_ws/README.md)의 launch 파일을 참고해 직접 작성해야 합니다.

## 통과 기준

- link는 물체, joint는 link 사이 관계라는 점을 설명한다.
- `base_link -> laser_link`가 없으면 LiDAR 데이터를 costmap에 넣기 어렵다는 점을 설명한다.
- `map -> odom`과 `base_link -> laser_link`가 서로 다른 성격의 TF라는 점을 설명한다.
