"""카메라–LiDAR 동기화, bounded VO, 독립 auditor를 한 번에 실행한다."""

from launch import LaunchDescription
# launch_ros.actions.Node는 설치된 ROS 2 실행 파일과 process 이름을 선언한다.
from launch_ros.actions import Node


def generate_launch_description():
    """각 노드를 별도 process로 띄워 topic/DDS 경계를 관찰 가능하게 유지한다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_12",
                executable="sensor_simulator",
                name="sensor_simulator",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_12",
                executable="approximate_time_sync",
                name="approximate_time_sync",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_12",
                executable="bounded_vo_estimator",
                name="bounded_vo_estimator",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_12",
                executable="vo_auditor",
                name="vo_auditor",
                output="screen",
            ),
        ]
    )
