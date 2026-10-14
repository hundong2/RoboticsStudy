from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """지연 센서, bounded OOSM 추정기, 독립 감사기를 재현 가능한 한 묶음으로 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_14",
            executable="delayed_sensor_simulator",
            name="delayed_sensor_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_14",
            executable="bounded_oosm_estimator",
            name="bounded_oosm_estimator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_14",
            executable="oosm_safety_auditor",
            name="oosm_safety_auditor",
            output="screen",
        ),
    ])
