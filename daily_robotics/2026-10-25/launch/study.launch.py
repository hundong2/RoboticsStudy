from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """30 Hz camera, bounded visual frontend, 독립 auditor를 함께 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_25",
            executable="camera_stream_simulator",
            name="camera_stream_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_25",
            executable="bounded_feature_tracker",
            name="bounded_feature_tracker",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_25",
            executable="visual_frontend_auditor",
            name="visual_frontend_auditor",
            output="screen",
        ),
    ])
