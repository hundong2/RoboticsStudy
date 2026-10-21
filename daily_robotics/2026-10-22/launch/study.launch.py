from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """합성 front-end, bounded back-end, 독립 auditor를 한 실습으로 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_22",
            executable="graph_input_simulator",
            name="graph_input_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_22",
            executable="bounded_switchable_optimizer",
            name="bounded_switchable_optimizer",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_22",
            executable="robust_slam_auditor",
            name="robust_slam_auditor",
            output="screen",
        ),
    ])
