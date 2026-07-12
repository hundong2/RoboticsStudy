# 02. Python ROS 2 노드 워크스페이스

이 프로젝트는 주석이 많은 Python ROS 2 패키지입니다. 초보자가 Python 문법과 ROS 2 API를 한 줄씩 읽을 수 있도록 작성했습니다.

## 구조

```text
ros2_ws/
  src/
    hello_robot_py/
      hello_robot_py/
        hello_publisher.py
        hello_subscriber.py
        parameter_timer.py
      launch/
        bringup.launch.py
      package.xml
      setup.py
      setup.cfg
```

## 빌드

```bash
cd ROS/projects/02_python_nodes_ws/ros2_ws
source /opt/ros/lyrical/setup.bash
colcon build --packages-select hello_robot_py
source install/setup.bash
```

## 실행

publisher:

```bash
ros2 run hello_robot_py hello_publisher
```

subscriber:

```bash
ros2 run hello_robot_py hello_subscriber
```

parameter timer:

```bash
ros2 run hello_robot_py parameter_timer --ros-args -p robot_name:=study_bot -p speed_limit:=0.2
```

launch:

```bash
ros2 launch hello_robot_py bringup.launch.py
```

## 관찰 명령어

```bash
ros2 node list
ros2 topic list -t
ros2 topic echo /robot_status
ros2 topic echo /cmd_vel
ros2 param list
ros2 param get /parameter_timer speed_limit
```

## 학습 포인트

- `setup.py`의 `entry_points`가 `ros2 run` 실행 이름을 만듭니다.
- publisher는 topic으로 메시지를 보냅니다.
- subscriber는 topic에서 메시지를 받아 callback을 실행합니다.
- parameter는 실행 중 바꿀 수 있는 설정값입니다.
- launch 파일은 여러 노드를 한 번에 시작합니다.
