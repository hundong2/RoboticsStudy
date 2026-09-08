"""목표 생성기 → 제약 MPC → 안전 관절 플랜트의 폐루프 실습을 실행한다."""

from launch import LaunchDescription
# Node action은 package/lib에 설치된 ROS 2 실행 파일을 별도 프로세스로 시작한다.
from launch_ros.actions import Node


def generate_launch_description():
    """세 노드를 시작하고 터미널에 각 노드의 상태 로그를 함께 표시한다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_09",
                executable="reference_generator",
                name="reference_generator",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_09",
                executable="constrained_mpc_controller",
                name="constrained_mpc_controller",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_09",
                executable="safe_joint_plant",
                name="safe_joint_plant",
                output="screen",
            ),
        ]
    )
