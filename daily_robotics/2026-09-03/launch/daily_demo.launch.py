"""오늘의 TF2 + A* 실습 노드 두 개를 한 번에 실행하는 launch 파일."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """ROS 2 launch가 실행할 노드와 파라미터를 선언한다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_03",
                executable="tf_goal_source",
                name="tf_goal_source",
                output="screen",
                parameters=[
                    {"goal_x_in_lidar": 11.0},
                    {"goal_y_in_lidar": 5.0},
                ],
            ),
            Node(
                package="daily_robotics_2026_09_03",
                executable="deterministic_astar_planner",
                name="deterministic_astar_planner",
                output="screen",
            ),
        ]
    )
