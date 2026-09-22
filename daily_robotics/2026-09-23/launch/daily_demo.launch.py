"""접촉 플랜트, 임피던스 제어기, 독립 에너지 안전 게이트를 별도 프로세스로 실행한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    # 프로세스를 분리하면 제어 명령이 안전 게이트를 우회할 수 없는 Topic 구조를 관찰할 수 있다.
    return LaunchDescription([
        Node(package="daily_robotics_2026_09_23", executable="contact_plant"),
        Node(package="daily_robotics_2026_09_23", executable="impedance_controller"),
        Node(package="daily_robotics_2026_09_23", executable="energy_safety_auditor"),
    ])
