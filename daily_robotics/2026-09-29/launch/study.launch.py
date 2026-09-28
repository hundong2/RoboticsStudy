from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """센서 시뮬레이터, fixed-lag smoother, 독립 감사기를 한 번에 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_09_29",
            executable="sensor_simulator",
            name="sensor_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_29",
            executable="fixed_lag_smoother",
            name="fixed_lag_smoother",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_29",
            executable="fusion_auditor",
            name="fusion_auditor",
            output="screen",
        ),
    ])
