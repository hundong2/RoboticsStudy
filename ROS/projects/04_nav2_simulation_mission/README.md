# 04. Nav2 시뮬레이션 미션

목표는 "지도 생성 -> 위치 추정 -> 목표 지점 이동 -> 실패 분석"의 전체 흐름을 경험하는 것입니다.

## 준비물

- ROS 2 Lyrical 또는 Jazzy
- Gazebo
- Nav2
- TurtleBot3 또는 직접 만든 차동 구동 로봇 모델

## 미션 A: SLAM

```bash
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py
ros2 launch slam_toolbox online_async_launch.py use_sim_time:=true
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```

수행할 일:

1. 로봇을 움직이며 맵을 만듭니다.
2. `/map`, `/scan`, `/tf`, `/odom` topic을 확인합니다.
3. 맵을 저장합니다.

## 미션 B: Localization + Navigation

```bash
ros2 launch nav2_bringup localization_launch.py map:=my_map.yaml use_sim_time:=true
ros2 launch nav2_bringup navigation_launch.py use_sim_time:=true
```

수행할 일:

1. RViz에서 initial pose를 지정합니다.
2. 목표 지점을 지정합니다.
3. global path와 local costmap을 관찰합니다.
4. 실패하면 `/tf`, `/scan`, costmap, lifecycle 상태를 순서대로 확인합니다.

## 실패 주입 실습

- `use_sim_time`을 한 노드만 false로 바꿔 TF 오류를 확인합니다.
- LiDAR frame 이름을 틀리게 설정해 costmap 반응을 확인합니다.
- inflation radius를 극단적으로 키워 경로 생성 실패를 관찰합니다.

## 제출물

- 맵 파일
- RViz 캡처
- topic graph
- 실패 1개에 대한 원인, 재현 절차, 수정 방법
