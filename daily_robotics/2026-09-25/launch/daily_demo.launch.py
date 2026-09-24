"""3축 궤적 Action 서버, 독립 감사 노드, 예제 클라이언트를 함께 실행한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    """DDS 발견 순서와 무관하게 클라이언트가 서버를 기다리는 학습용 launch를 만든다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_25",
                executable="bounded_trajectory_server",
                name="bounded_trajectory_server",
                output="screen",
                parameters=[{"control_frequency_hz": 500.0, "rt_priority": 60}],
            ),
            Node(
                package="daily_robotics_2026_09_25",
                executable="tracking_guard",
                name="tracking_guard",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_25",
                executable="trajectory_goal_client",
                name="trajectory_goal_client",
                output="screen",
            ),
        ]
    )
