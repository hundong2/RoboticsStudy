from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """오늘 실습의 시뮬레이터, bounded smoother, 독립 auditor를 함께 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_20",
            executable="pose_factor_simulator",
            name="pose_factor_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_20",
            executable="bounded_pose_smoother",
            name="bounded_pose_smoother",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_20",
            executable="fusion_auditor",
            name="fusion_auditor",
            output="screen",
        ),
    ])
