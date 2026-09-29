from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """접촉 생성, 추정, 폴백, 독립 감사를 하나의 재현 가능한 실습으로 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_09_30",
            executable="contact_sensor_simulator",
            name="contact_sensor_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_30",
            executable="contact_covariance_estimator",
            name="contact_covariance_estimator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_30",
            executable="friction_fallback_supervisor",
            name="friction_fallback_supervisor",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_30",
            executable="contact_safety_auditor",
            name="contact_safety_auditor",
            output="screen",
        ),
    ])
