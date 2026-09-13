"""반응형 navigation, elastic band, deadline supervisor, auditor를 한 번에 실행한다."""

from launch import LaunchDescription
# launch_ros.actions.Node는 설치된 ROS 2 실행 파일을 개별 process로 실행한다.
from launch_ros.actions import Node


def generate_launch_description():
    """통신과 장애 격리를 관찰할 수 있도록 다섯 노드를 별도 process로 띄운다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_14",
                executable="world_simulator",
                name="world_simulator",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_14",
                executable="elastic_band_optimizer",
                name="elastic_band_optimizer",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_14",
                executable="reactive_bt_navigator",
                name="reactive_bt_navigator",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_14",
                executable="deadline_supervisor",
                name="deadline_supervisor",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_14",
                executable="trajectory_auditor",
                name="trajectory_auditor",
                output="screen",
            ),
        ]
    )
