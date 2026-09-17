"""세 노드를 한 프로세스씩 실행하여 Topic 흐름을 관찰한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    # 각 Node는 패키지에서 설치한 실행 파일을 별도 프로세스로 시작한다.
    return LaunchDescription([
        Node(package="daily_robotics_2026_09_18", executable="scan_simulator"),
        Node(package="daily_robotics_2026_09_18", executable="bounded_grid_mapper"),
        Node(package="daily_robotics_2026_09_18", executable="map_auditor"),
    ])
