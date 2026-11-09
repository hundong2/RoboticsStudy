from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """고정 map/이동 scan source, bounded NDT, 독립 auditor를 함께 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_11_10",
            executable="pointcloud_pair_source",
            name="pointcloud_pair_source",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_11_10",
            executable="bounded_ndt_matcher",
            name="bounded_ndt_matcher",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_11_10",
            executable="ndt_auditor",
            name="ndt_auditor",
            output="screen",
        ),
    ])
