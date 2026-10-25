from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """30 Hz stereo 관측, bounded RANSAC odometry, 독립 auditor를 함께 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_26",
            executable="stereo_correspondence_simulator",
            name="stereo_correspondence_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_26",
            executable="bounded_stereo_odometry",
            name="bounded_stereo_odometry",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_26",
            executable="stereo_odometry_auditor",
            name="stereo_odometry_auditor",
            output="screen",
        ),
    ])
