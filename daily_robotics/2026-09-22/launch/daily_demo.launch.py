"""액추에이터 시뮬레이터, IMM 안전 감독기, 독립 auditor를 별도 프로세스로 실행한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    # 프로세스를 분리해야 Topic QoS의 Deadline/Liveliness 장애가 실제 DDS 경계를 통과한다.
    return LaunchDescription([
        Node(package="daily_robotics_2026_09_22", executable="actuator_simulator"),
        Node(package="daily_robotics_2026_09_22", executable="imm_fault_supervisor"),
        Node(package="daily_robotics_2026_09_22", executable="safety_auditor"),
    ])
