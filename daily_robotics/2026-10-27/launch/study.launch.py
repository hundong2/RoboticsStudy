from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    """100 Hz 이벤트 packet, bounded contrast estimator, 독립 auditor를 실행한다."""
    return LaunchDescription([
        Node(
            package="daily_robotics_2026_10_27",
            executable="event_packet_simulator",
            name="event_packet_simulator",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_27",
            executable="bounded_contrast_flow",
            name="bounded_contrast_flow",
            output="screen",
        ),
        Node(
            package="daily_robotics_2026_10_27",
            executable="event_flow_auditor",
            name="event_flow_auditor",
            output="screen",
        ),
    ])
