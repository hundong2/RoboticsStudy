from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """3R arm, SNS 제어기, 독립 감사기를 한 번에 실행하는 재현 가능한 실습이다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_01",
            executable="redundant_arm_simulator",
            name="redundant_arm_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_01",
            executable="sns_joint_velocity_controller",
            name="sns_joint_velocity_controller",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_01",
            executable="sns_safety_auditor",
            name="sns_safety_auditor",
            output="screen",
        ),
    ])
