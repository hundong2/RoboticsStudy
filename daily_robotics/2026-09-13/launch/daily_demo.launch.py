from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch_ros.actions import Node


def generate_launch_description():
    """하드웨어 configure→activate, broadcaster/controller 순서를 재현 가능한 시간축으로 실행한다."""
    package_share = Path(get_package_share_directory("daily_robotics_2026_09_13"))
    robot_description = (package_share / "description" / "study_arm.urdf").read_text()
    controller_config = str(package_share / "config" / "controllers.yaml")

    # 최신 ros2_control은 parameter 직접 주입 대신 robot_state_publisher의 transient-local topic을 구독한다.
    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        parameters=[{"robot_description": robot_description}],
        output="screen",
    )

    controller_manager = Node(
        package="controller_manager",
        executable="ros2_control_node",
        parameters=[controller_config],
        output="screen",
    )

    # hardware_spawner는 서비스가 준비될 때까지 기다린 뒤 configure→activate를 순서대로 수행한다.
    activate_hardware = Node(
        package="controller_manager",
        executable="hardware_spawner",
        arguments=["StudyArm", "--activate", "--controller-manager", "/controller_manager"],
        output="screen",
    )
    spawn_broadcaster = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_state_broadcaster", "--controller-manager", "/controller_manager"],
        output="screen",
    )
    # spawner는 load→configure→activate를 호출한다. state broadcaster를 먼저 올려 검증 입력을 확보한다.
    spawn_ik = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["ik_controller", "--controller-manager", "/controller_manager"],
        output="screen",
    )
    auditor = Node(
        package="daily_robotics_2026_09_13",
        executable="target_auditor",
        output="screen",
    )

    # OnProcessExit를 사용해 wall-time 추측 대신 실제 완료 사건으로 lifecycle 순서를 보장한다.
    after_hardware = RegisterEventHandler(
        OnProcessExit(target_action=activate_hardware, on_exit=[spawn_broadcaster])
    )
    after_broadcaster = RegisterEventHandler(
        OnProcessExit(target_action=spawn_broadcaster, on_exit=[spawn_ik])
    )
    after_ik = RegisterEventHandler(
        OnProcessExit(target_action=spawn_ik, on_exit=[auditor])
    )

    return LaunchDescription([
        robot_state_publisher,
        controller_manager,
        activate_hardware,
        after_hardware,
        after_broadcaster,
        after_ik,
    ])
