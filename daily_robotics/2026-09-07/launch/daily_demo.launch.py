"""세 composable node를 intra-process 통신이 켜진 한 컨테이너에 로드한다."""

from launch import LaunchDescription
# ComposableNodeContainer는 별도 실행 파일 대신 공유 라이브러리 노드를 한 프로세스에 올린다.
from launch_ros.actions import ComposableNodeContainer
# ComposableNode는 로드할 plugin, 이름, 파라미터, intra-process 옵션을 선언한다.
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    """센서 → DWA → 명령 가드 파이프라인을 재현 가능한 설정으로 실행한다."""
    # 모든 컴포넌트에 같은 옵션을 주어 같은 프로세스의 단일 구독자 경로에서
    # std::unique_ptr 메시지 소유권을 넘길 수 있게 한다.
    intra_process = [{"use_intra_process_comms": True}]

    container = ComposableNodeContainer(
        name="dwa_component_container",
        namespace="",
        package="rclcpp_components",
        # component_container_mt는 MultiThreadedExecutor를 사용한다. 각 노드의 기본
        # MutuallyExclusive callback group은 같은 노드 내부 상태의 동시 접근을 막는다.
        executable="component_container_mt",
        output="screen",
        composable_node_descriptions=[
            ComposableNode(
                package="daily_robotics_2026_09_07",
                plugin=(
                    "daily_robotics_2026_09_07::"
                    "SensorSimulatorComponent"
                ),
                name="sensor_simulator",
                extra_arguments=intra_process,
            ),
            ComposableNode(
                package="daily_robotics_2026_09_07",
                plugin="daily_robotics_2026_09_07::DwaPlannerComponent",
                name="dwa_planner",
                # 처음에는 목표를 로봇의 정면보다 약간 왼쪽에 둔다.
                parameters=[
                    {
                        "goal_x": 3.0,
                        "goal_y": 0.8,
                        "heading_weight": 0.45,
                        "clearance_weight": 0.35,
                        "speed_weight": 0.20,
                    }
                ],
                extra_arguments=intra_process,
            ),
            ComposableNode(
                package="daily_robotics_2026_09_07",
                plugin="daily_robotics_2026_09_07::CommandGuardComponent",
                name="command_guard",
                extra_arguments=intra_process,
            ),
        ],
    )

    return LaunchDescription([container])
