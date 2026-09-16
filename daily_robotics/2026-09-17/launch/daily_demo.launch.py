"""IMU 입력, 사전적분, 독립 오차 감사를 각각 별도 프로세스로 실행한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """/clock 발행자는 벽시계로 동작하고 소비자만 ROS 시뮬레이션 시간을 사용한다."""
    return LaunchDescription([
        Node(package="daily_robotics_2026_09_17", executable="imu_simulator",
             name="imu_simulator", output="screen"),
        Node(package="daily_robotics_2026_09_17", executable="imu_preintegrator",
             name="imu_preintegrator", output="screen",
             parameters=[{"use_sim_time": True}]),
        Node(package="daily_robotics_2026_09_17", executable="integration_auditor",
             name="integration_auditor", output="screen",
             parameters=[{"use_sim_time": True}]),
    ])
