from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """합성 actuator, 공분산 보정 필터, 안전 감독, 독립 auditor를 함께 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_23",
            executable="actuator_sensor_simulator",
            name="actuator_sensor_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_23",
            executable="adaptive_covariance_filter",
            name="adaptive_covariance_filter",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_23",
            executable="energy_rate_supervisor",
            name="energy_rate_supervisor",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_23",
            executable="pipeline_auditor",
            name="pipeline_auditor",
            output="screen",
        ),
    ])
