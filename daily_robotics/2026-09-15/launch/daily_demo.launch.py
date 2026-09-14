"""관절 공간 world, RRT-Connect, priority-safe servo, auditor를 한 번에 실행한다."""

from launch import LaunchDescription
# launch_ros.actions.Node는 설치된 ROS 2 실행 파일을 각각 독립 process로 실행한다.
from launch_ros.actions import Node


def generate_launch_description():
    """네 노드의 Topic 계약과 고/저우선순위 실행 분리를 관찰한다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_15",
                executable="joint_space_world",
                name="joint_space_world",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_15",
                executable="bounded_rrt_connect",
                name="bounded_rrt_connect",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_15",
                executable="priority_safe_servo",
                name="priority_safe_servo",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_15",
                executable="trajectory_auditor",
                name="trajectory_auditor",
                output="screen",
            ),
        ]
    )
