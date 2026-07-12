#!/usr/bin/env python3
"""A small ROS 2 subscriber with beginner-focused comments.

subscriber는 topic에서 message를 받아 callback을 실행하는 노드입니다.
이 파일은 /robot_status topic을 구독하고 들어온 문자열을 로그로 출력합니다.
"""

# ROS 2 Python 기능을 사용하기 위해 rclpy를 import합니다.
import rclpy

# Node는 ROS 2 노드의 공통 기능을 담은 클래스입니다.
from rclpy.node import Node

# publisher가 보내는 것과 같은 메시지 타입을 subscriber도 알아야 합니다.
from std_msgs.msg import String


# HelloSubscriber는 Node를 상속받아 ROS 그래프에 참여하는 노드가 됩니다.
class HelloSubscriber(Node):
    """Subscribe to /robot_status and log every received message."""

    # 생성자는 노드 이름, subscription, 상태 변수를 준비합니다.
    def __init__(self):
        # 부모 Node 초기화입니다. 노드 이름은 hello_subscriber입니다.
        super().__init__("hello_subscriber")

        # received_count는 받은 메시지 개수를 세는 정수 변수입니다.
        self.received_count = 0

        # create_subscription은 topic을 구독하는 subscription 객체를 만듭니다.
        # 첫 번째 인자는 메시지 타입입니다.
        # 두 번째 인자는 topic 이름입니다.
        # 세 번째 인자는 메시지가 도착할 때 호출될 callback 함수입니다.
        # 네 번째 인자는 queue depth입니다.
        self.subscription = self.create_subscription(
            String,
            "/robot_status",
            self.on_status_message,
            10,
        )

        # subscription 변수를 쓰지 않는다고 linter가 오해하지 않도록 객체에 보관합니다.
        # ROS 2에서는 subscription 객체가 사라지면 구독도 사라질 수 있습니다.
        self.get_logger().info("subscriber started and waiting for /robot_status")

    # 이 함수는 /robot_status 메시지가 들어올 때마다 자동 호출됩니다.
    # message 매개변수에는 방금 받은 String 메시지 객체가 들어옵니다.
    def on_status_message(self, message):
        # 받은 메시지 개수를 1 증가시킵니다.
        self.received_count += 1

        # message.data에는 publisher가 넣은 문자열이 들어 있습니다.
        self.get_logger().info(
            f"received #{self.received_count}: {message.data}"
        )


# main은 실행 진입점입니다.
def main(args=None):
    # ROS 2 Python 라이브러리를 초기화합니다.
    rclpy.init(args=args)

    # subscriber 노드 객체를 만듭니다.
    node = HelloSubscriber()

    try:
        # spin은 subscription callback이 실행될 수 있도록 이벤트 루프를 돌립니다.
        rclpy.spin(node)
    except KeyboardInterrupt:
        # Ctrl+C 종료는 정상적인 학습 흐름이므로 오류로 처리하지 않습니다.
        pass
    finally:
        # 노드 리소스를 정리합니다.
        node.destroy_node()

        # rclpy를 종료합니다.
        rclpy.shutdown()


# 직접 실행될 때 main 함수를 호출합니다.
if __name__ == "__main__":
    main()
