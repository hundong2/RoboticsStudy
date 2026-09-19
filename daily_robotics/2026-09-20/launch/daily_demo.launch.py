"""고정 크기 센서→예산 감시/통과성 계산→독립 감사 파이프라인을 실행한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    # 각 Node를 별도 프로세스로 두어 Topic 직렬화 경계와 실제 ROS graph를 관찰한다.
    return LaunchDescription([
        Node(package="daily_robotics_2026_09_20", executable="mcu_let_source"),
        Node(package="daily_robotics_2026_09_20", executable="transport_budget_monitor"),
        Node(package="daily_robotics_2026_09_20", executable="traversability_estimator"),
        Node(package="daily_robotics_2026_09_20", executable="pipeline_auditor"),
    ])
