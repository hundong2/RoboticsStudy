from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """센서 시뮬레이터, bounded 접촉 EKF, 독립 auditor를 함께 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_21",
            executable="contact_sensor_simulator",
            name="contact_sensor_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_21",
            executable="bounded_contact_ekf",
            name="bounded_contact_ekf",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_21",
            executable="contact_estimator_auditor",
            name="contact_estimator_auditor",
            output="screen",
        ),
    ])
