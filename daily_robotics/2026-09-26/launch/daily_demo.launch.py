"""Wrench 명령, bounded 접촉력 QP, 독립 감사를 한 번에 실행한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    """동일한 마찰·기하 파라미터를 allocator와 auditor에 주입한다."""
    shared_contact = {
        "friction_coefficient": 0.6,
        "min_normal_force": 15.0,
        "max_normal_force": 180.0,
        "contact_half_span": 0.25,
        "com_height": 0.55,
    }

    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_26",
                executable="wrench_command_publisher",
                name="wrench_command_publisher",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_26",
                executable="bounded_contact_allocator",
                name="bounded_contact_allocator",
                output="screen",
                parameters=[
                    shared_contact,
                    {
                        "kernel_frequency_hz": 200.0,
                        "solver_iterations": 64,
                        "smooth_weight": 0.02,
                    },
                ],
            ),
            Node(
                package="daily_robotics_2026_09_26",
                executable="whole_body_auditor",
                name="whole_body_auditor",
                output="screen",
                parameters=[shared_contact],
            ),
        ]
    )
