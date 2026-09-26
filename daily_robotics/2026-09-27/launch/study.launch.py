from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """시뮬레이터 → deskew → 독립 감사 노드를 한 번에 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_09_27",
            executable="lidar_imu_simulator",
            name="lidar_imu_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_27",
            executable="imu_deskew_node",
            name="imu_deskew_node",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_27",
            executable="deskew_auditor",
            name="deskew_auditor",
            output="screen",
        ),
    ])
