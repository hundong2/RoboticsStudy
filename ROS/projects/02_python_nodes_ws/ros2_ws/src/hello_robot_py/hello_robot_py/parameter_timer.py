#!/usr/bin/env python3
"""A parameter-driven timer node with beginner-focused comments.

이 노드는 parameter 값을 읽어 /cmd_vel 속도 명령을 주기적으로 publish합니다.
실제 로봇에 연결하기 전에 turtlesim이나 시뮬레이터에서만 먼저 실험하세요.
"""

# rclpy는 ROS 2 Python API입니다.
import rclpy

# Node는 모든 ROS 2 Python 노드의 기반 클래스입니다.
from rclpy.node import Node

# Twist는 선속도(linear)와 각속도(angular)를 표현하는 표준 메시지입니다.
from geometry_msgs.msg import Twist


# ParameterTimer는 Node를 상속받는 사용자 정의 노드 클래스입니다.
class ParameterTimer(Node):
    """Publish /cmd_vel using runtime parameters."""

    # __init__은 노드 객체를 만들 때 한 번 실행됩니다.
    def __init__(self):
        # 부모 Node를 parameter_timer라는 이름으로 초기화합니다.
        super().__init__("parameter_timer")

        # robot_name은 로그에 표시할 로봇 이름입니다.
        self.declare_parameter("robot_name", "study_bot")

        # speed_limit은 전진 속도 제한입니다. 단위는 m/s로 가정합니다.
        self.declare_parameter("speed_limit", 0.1)

        # angular_limit은 회전 속도 제한입니다. 단위는 rad/s로 가정합니다.
        self.declare_parameter("angular_limit", 0.2)

        # enable_motion은 실제로 속도 명령을 보낼지 결정하는 boolean parameter입니다.
        self.declare_parameter("enable_motion", True)

        # /cmd_vel은 모바일 로봇에서 널리 쓰이는 속도 명령 topic 이름입니다.
        self.velocity_publisher = self.create_publisher(Twist, "/cmd_vel", 10)

        # 0.5초마다 publish_velocity 함수를 호출합니다.
        self.timer = self.create_timer(0.5, self.publish_velocity)

        # 시작 로그를 남깁니다.
        self.get_logger().info("parameter_timer started")

    # parameter는 실행 중 ros2 param set으로 바뀔 수 있으므로 callback마다 다시 읽습니다.
    def publish_velocity(self):
        # 문자열 parameter를 읽습니다.
        robot_name = self.get_parameter("robot_name").value

        # 실수 parameter를 읽습니다.
        speed_limit = self.get_parameter("speed_limit").value
        angular_limit = self.get_parameter("angular_limit").value

        # boolean parameter를 읽습니다.
        enable_motion = self.get_parameter("enable_motion").value

        # Twist 메시지 객체를 만듭니다.
        command = Twist()

        # if 문은 조건이 True일 때 들여쓰기된 블록을 실행합니다.
        if enable_motion:
            # linear.x는 로봇 전방 속도를 의미하는 관례가 많습니다.
            command.linear.x = float(speed_limit)

            # angular.z는 평면 로봇의 회전 속도를 의미하는 관례가 많습니다.
            command.angular.z = float(angular_limit)
        else:
            # enable_motion이 False면 기본값 0.0을 유지해 정지 명령을 보냅니다.
            command.linear.x = 0.0
            command.angular.z = 0.0

        # 완성된 Twist 메시지를 /cmd_vel topic으로 publish합니다.
        self.velocity_publisher.publish(command)

        # 로그로 현재 parameter와 publish 값을 확인합니다.
        self.get_logger().info(
            f"{robot_name}: enable_motion={enable_motion}, "
            f"linear.x={command.linear.x:.3f}, angular.z={command.angular.z:.3f}"
        )


# main 함수는 setup.py의 entry_points에서 참조합니다.
def main(args=None):
    # ROS 2 Python 런타임을 시작합니다.
    rclpy.init(args=args)

    # 노드 객체를 생성합니다.
    node = ParameterTimer()

    try:
        # callback 처리를 위해 노드를 계속 실행합니다.
        rclpy.spin(node)
    except KeyboardInterrupt:
        # 사용자가 Ctrl+C로 종료하면 조용히 빠져나갑니다.
        pass
    finally:
        # 종료 전 0 속도 명령을 한 번 보내는 것이 실제 로봇에서는 더 안전합니다.
        stop_command = Twist()
        node.velocity_publisher.publish(stop_command)

        # 노드와 rclpy 리소스를 정리합니다.
        node.destroy_node()
        rclpy.shutdown()


# 직접 실행된 경우 main을 호출합니다.
if __name__ == "__main__":
    main()
