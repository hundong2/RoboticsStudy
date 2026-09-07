"""차동구동 모터 경로와 파티클 필터 위치추정을 한 번에 실행한다."""

from launch import LaunchDescription
# Node action은 package/lib 아래 설치된 ROS 2 실행 파일을 프로세스로 시작한다.
from launch_ros.actions import Node


def generate_launch_description():
    """센서/명령 모의 → 우선순위 모터 제어 → MCL 파이프라인을 시작한다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_08",
                executable="sensor_simulator",
                name="sensor_simulator",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_08",
                executable="priority_motor_controller",
                name="priority_motor_controller",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_08",
                executable="particle_localizer",
                name="particle_localizer",
                output="screen",
            ),
        ]
    )
