"""이동 라이다, deskew mapper, 독립 품질 auditor를 별도 프로세스로 실행한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    # 프로세스를 분리하면 TF/DDS 경계를 실제 ROS graph에서 관찰할 수 있다.
    return LaunchDescription([
        Node(package="daily_robotics_2026_09_21", executable="moving_scan_simulator"),
        Node(package="daily_robotics_2026_09_21", executable="motion_compensated_mapper"),
        Node(package="daily_robotics_2026_09_21", executable="map_quality_auditor"),
    ])
