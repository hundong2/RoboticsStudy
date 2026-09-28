from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """3-DoF plant, operational-space controller, independent auditor를 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_09_28",
            executable="arm_plant",
            name="arm_plant",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_28",
            executable="operational_space_controller",
            name="operational_space_controller",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_09_28",
            executable="control_auditor",
            name="control_auditor",
            output="screen",
        ),
    ])
