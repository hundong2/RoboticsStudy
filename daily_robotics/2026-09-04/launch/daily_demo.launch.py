"""URDF + WaitSet + 평면 2R IK 실습 전체를 실행하는 ROS 2 launch 파일."""

from pathlib import Path

# ament index는 설치된 패키지의 share 디렉터리를 이름으로 찾아준다.
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
# launch_ros.actions.Node는 ROS 2 노드 실행 파일과 파라미터를 선언한다.
from launch_ros.actions import Node


def generate_launch_description():
    """세 노드와 동일한 링크 길이 파라미터를 하나의 실행 구성으로 묶는다."""
    package_share = Path(
        get_package_share_directory("daily_robotics_2026_09_04")
    )

    # robot_state_publisher는 XML 문자열 형태의 robot_description 파라미터를 요구한다.
    robot_description = (package_share / "urdf" / "two_link_arm.urdf").read_text(
        encoding="utf-8"
    )

    return LaunchDescription(
        [
            # /joint_states와 URDF를 결합해 base_link→link_1→link_2→tool0 TF를 발행한다.
            Node(
                package="robot_state_publisher",
                executable="robot_state_publisher",
                name="robot_state_publisher",
                output="screen",
                parameters=[{"robot_description": robot_description}],
            ),
            # 도달 가능한 원형 /arm/target을 2 Hz로 발행한다.
            Node(
                package="daily_robotics_2026_09_04",
                executable="circular_target_publisher",
                name="circular_target_publisher",
                output="screen",
            ),
            # WaitSet으로 목표를 직접 take하고 IK 결과를 /joint_states로 보낸다.
            Node(
                package="daily_robotics_2026_09_04",
                executable="ik_waitset_controller",
                name="ik_waitset_controller",
                output="screen",
                parameters=[
                    {"link_1_length": 0.5},
                    {"link_2_length": 0.4},
                    {"positive_elbow_branch": True},
                ],
            ),
        ]
    )
