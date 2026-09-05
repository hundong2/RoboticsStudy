"""Lifecycle + QoS fault detection + planar EKF 실습 노드 세 개를 함께 실행한다."""

from launch import LaunchDescription
# launch_ros.actions.Node는 설치된 ROS 2 실행 파일을 별도 프로세스로 시작한다.
from launch_ros.actions import Node


def generate_launch_description():
    """센서, 관리 대상 EKF, 외부 상태 관리자를 하나의 재현 가능한 데모로 구성한다."""
    return LaunchDescription(
        [
            # 50 Hz wheel/gyro와 5 Hz GPS를 만들고, 주기적으로 wheel 장애를 주입한다.
            Node(
                package="daily_robotics_2026_09_06",
                executable="sensor_simulator",
                name="sensor_simulator",
                output="screen",
            ),
            # configure 전에는 자원을 만들지 않고 activate 뒤에만 추정치를 내는 관리 노드다.
            Node(
                package="daily_robotics_2026_09_06",
                executable="lifecycle_ekf_node",
                name="planar_ekf",
                output="screen",
            ),
            # 표준 ChangeState Service로 configure→activate를 순서대로 요청한다.
            Node(
                package="daily_robotics_2026_09_06",
                executable="lifecycle_manager",
                name="ekf_lifecycle_manager",
                output="screen",
            ),
        ]
    )
