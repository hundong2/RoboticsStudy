# 03. Python 패키지와 노드

Python은 ROS 2 입문에 가장 적합한 언어입니다. 빌드가 빠르고 문법이 간단해 Node, Topic, Parameter, Launch의 구조를 익히기 좋습니다.

## 패키지 생성

```bash
cd ~/ros2_ws/src
ros2 pkg create hello_robot_py --build-type ament_python --dependencies rclpy std_msgs geometry_msgs
cd ~/ros2_ws
colcon build --packages-select hello_robot_py
source install/setup.bash
```

## Python ROS 2 노드의 기본 구조

```python
import rclpy
from rclpy.node import Node

class MyNode(Node):
    def __init__(self):
        super().__init__("my_node")

def main():
    rclpy.init()
    node = MyNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
```

## 구조 해석

- `import`는 외부 모듈을 현재 파일에서 쓰기 위해 불러옵니다.
- `class MyNode(Node)`는 `Node` 기능을 상속받아 새 노드 클래스를 만듭니다.
- `__init__`은 객체가 생성될 때 한 번 실행됩니다.
- `super().__init__("my_node")`는 부모 클래스인 `Node`를 초기화합니다.
- `rclpy.init()`은 ROS 2 Python 클라이언트 라이브러리를 시작합니다.
- `rclpy.spin(node)`는 callback이 실행될 수 있도록 노드를 계속 돌립니다.
- `destroy_node()`와 `shutdown()`은 종료 정리입니다.

## 실습 위치

실제 주석 많은 코드는 [projects/02_python_nodes_ws](../projects/02_python_nodes_ws/README.md)에 있습니다.

## 실행 흐름

1. publisher 노드는 일정 시간마다 문자열 메시지를 보냅니다.
2. subscriber 노드는 같은 topic을 구독하고 메시지를 로그로 출력합니다.
3. parameter 노드는 실행 중 설정값을 읽어 동작을 바꿉니다.
4. launch 파일은 여러 노드를 한 번에 실행합니다.

## 통과 기준

- `setup.py`의 `entry_points`가 `ros2 run` 명령과 연결된다는 점을 설명한다.
- timer callback과 subscription callback의 차이를 설명한다.
- parameter를 코드 상수 대신 런타임 설정으로 쓰는 이유를 설명한다.
