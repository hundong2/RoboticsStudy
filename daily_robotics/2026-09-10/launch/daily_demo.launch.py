"""두 로봇의 독립 frontier 탐사 파이프라인과 fleet 관찰 노드를 실행한다."""

from launch import LaunchDescription
# GroupAction은 같은 namespace/설정을 적용할 여러 action을 논리적으로 묶는다.
from launch.actions import GroupAction
# PushRosNamespace는 그룹 안의 상대 이름 앞에 /robot_1 또는 /robot_2를 붙인다.
from launch_ros.actions import Node, PushRosNamespace


def robot_group(robot_namespace: str, robot_x: float, robot_y: float):
    """같은 실행 파일을 namespace와 parameter만 바꿔 로봇 한 대의 stack으로 재사용한다."""
    return GroupAction(
        [
            PushRosNamespace(robot_namespace),
            Node(
                package="daily_robotics_2026_09_10",
                executable="map_simulator",
                name="map_simulator",
                output="screen",
                parameters=[
                    {
                        "robot_id": robot_namespace,
                        "robot_x": robot_x,
                        "robot_y": robot_y,
                    }
                ],
                # 소스의 상대 이름 local_map을 배포 인터페이스 map으로 바꾼다.
                # namespace가 먼저 적용되어 최종 FQN은 /robot_N/map이 된다.
                remappings=[("local_map", "map")],
            ),
            Node(
                package="daily_robotics_2026_09_10",
                executable="bounded_frontier_explorer",
                name="frontier_explorer",
                output="screen",
                parameters=[{"robot_x": robot_x, "robot_y": robot_y}],
                # 알고리즘 코드는 generic 이름을 쓰고 launch가 실제 graph 계약을 정한다.
                remappings=[
                    ("map_input", "map"),
                    ("goal_output", "frontier_goal"),
                ],
            ),
        ]
    )


def generate_launch_description():
    """robot_1, robot_2와 전역 fleet coordinator를 한 번에 시작한다."""
    return LaunchDescription(
        [
            robot_group("robot_1", -1.50, -0.50),
            robot_group("robot_2", 1.50, 0.50),
            Node(
                package="daily_robotics_2026_09_10",
                executable="fleet_coordinator",
                namespace="fleet",
                name="coordinator",
                output="screen",
            ),
        ]
    )
