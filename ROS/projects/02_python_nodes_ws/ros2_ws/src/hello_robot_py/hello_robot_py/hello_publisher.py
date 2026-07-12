#!/usr/bin/env python3
"""A small ROS 2 publisher with beginner-focused comments.

publisher는 topic에 message를 보내는 노드입니다.
이 파일은 /robot_status topic에 std_msgs/String 메시지를 1초마다 보냅니다.
"""

# rclpy는 ROS 2의 Python 클라이언트 라이브러리입니다.
# Python 코드에서 node, publisher, subscriber, timer를 만들 때 사용합니다.
import rclpy

# Node 클래스는 ROS 2 노드의 기본 기능을 제공합니다.
# 우리가 만드는 클래스는 Node를 상속해서 ROS 노드가 됩니다.
from rclpy.node import Node

# String은 ROS 2 표준 메시지 타입입니다.
# std_msgs/msg/String은 문자열 하나를 data 필드에 담습니다.
from std_msgs.msg import String


# class는 관련 데이터와 함수를 하나로 묶는 Python 문법입니다.
# HelloPublisher(Node)는 HelloPublisher가 Node의 기능을 물려받는다는 뜻입니다.
class HelloPublisher(Node):
    """Publish a simple heartbeat message on /robot_status."""

    # __init__은 객체가 만들어질 때 자동으로 호출되는 특별한 메서드입니다.
    # self는 "지금 만들어진 이 객체 자신"을 가리키는 Python 관례 이름입니다.
    def __init__(self):
        # super()는 부모 클래스(Node)의 기능에 접근합니다.
        # Node 생성자에는 ROS 그래프에서 보일 노드 이름을 문자열로 넘깁니다.
        super().__init__("hello_publisher")

        # declare_parameter는 노드가 사용할 설정값을 등록합니다.
        # robot_name이라는 parameter가 없으면 기본값 study_bot을 사용합니다.
        self.declare_parameter("robot_name", "study_bot")

        # get_parameter(...).value는 parameter의 실제 값을 꺼냅니다.
        # self.robot_name은 이 객체의 다른 메서드에서도 쓸 수 있는 인스턴스 변수입니다.
        self.robot_name = self.get_parameter("robot_name").value

        # create_publisher는 topic에 메시지를 보낼 publisher 객체를 만듭니다.
        # 첫 번째 인자는 메시지 타입, 두 번째 인자는 topic 이름, 세 번째 인자는 queue depth입니다.
        self.publisher = self.create_publisher(String, "/robot_status", 10)

        # count는 몇 번째 메시지를 보내는지 기록하는 정수 변수입니다.
        self.count = 0

        # create_timer는 일정 주기마다 함수를 실행합니다.
        # 1.0은 1초 주기이고, self.publish_status는 호출할 callback 함수입니다.
        self.timer = self.create_timer(1.0, self.publish_status)

        # get_logger().info는 ROS 로그를 남깁니다.
        # f-string은 문자열 안에 변수 값을 넣는 Python 문법입니다.
        self.get_logger().info(f"{self.robot_name} publisher started")

    # publish_status는 timer가 1초마다 호출하는 callback 메서드입니다.
    def publish_status(self):
        # String()은 std_msgs/msg/String 메시지 객체를 새로 만듭니다.
        message = String()

        # message.data는 String 메시지의 실제 문자열 필드입니다.
        message.data = f"{self.robot_name} heartbeat {self.count}"

        # publish는 topic으로 message를 전송합니다.
        self.publisher.publish(message)

        # 로그는 학습 중 데이터 흐름을 확인하는 가장 쉬운 방법입니다.
        self.get_logger().info(f"published: {message.data}")

        # += 1은 기존 값에 1을 더해 다시 저장하는 Python 축약 문법입니다.
        self.count += 1


# main 함수는 ros2 run으로 이 파일을 실행할 때 진입점이 됩니다.
# args=None은 인자를 받지 않으면 기본값 None을 쓰겠다는 뜻입니다.
def main(args=None):
    # rclpy.init은 ROS 2 Python 시스템을 초기화합니다.
    # 이 호출 이후에 Node를 만들고 ROS 통신을 사용할 수 있습니다.
    rclpy.init(args=args)

    # HelloPublisher 클래스의 객체를 만듭니다.
    node = HelloPublisher()

    try:
        # spin은 노드가 timer, subscription 같은 callback을 계속 처리하게 합니다.
        rclpy.spin(node)
    except KeyboardInterrupt:
        # Ctrl+C가 눌리면 KeyboardInterrupt 예외가 발생합니다.
        # pass는 "아무 것도 하지 않고 넘어간다"는 Python 문법입니다.
        pass
    finally:
        # finally는 예외 발생 여부와 관계없이 마지막에 실행됩니다.
        # destroy_node는 ROS 노드 리소스를 정리합니다.
        node.destroy_node()

        # shutdown은 rclpy 전체를 종료합니다.
        rclpy.shutdown()


# __name__은 Python이 현재 파일을 어떻게 실행했는지 알려주는 특수 변수입니다.
# 이 파일을 직접 실행하면 "__main__"이고, import되면 모듈 이름이 됩니다.
if __name__ == "__main__":
    main()
