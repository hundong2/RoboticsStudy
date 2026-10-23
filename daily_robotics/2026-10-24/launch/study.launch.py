from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """200 Hz IMU, 20 Hz visual keyframe, bounded smoother, 독립 auditor를 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_24",
            executable="vio_sensor_simulator",
            name="vio_sensor_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_24",
            executable="bounded_preintegration_smoother",
            name="bounded_preintegration_smoother",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_24",
            executable="vio_auditor",
            name="vio_auditor",
            output="screen",
        ),
    ])
