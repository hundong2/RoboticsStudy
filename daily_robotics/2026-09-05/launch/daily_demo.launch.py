"""ROS 2 Action + RT 제어 경계 + 2D ICP 실습 노드 다섯 개를 함께 실행한다."""

from launch import LaunchDescription
# launch_ros.actions.Node는 패키지에 설치된 ROS 2 실행 파일을 프로세스로 시작한다.
from launch_ros.actions import Node


def generate_launch_description():
    """제어 파이프라인과 scan matching 파이프라인을 한 번에 구성한다."""
    return LaunchDescription(
        [
            # /cmd_vel을 100 Hz로 적분해 /wheel/odom을 돌려주는 간단한 이동 베이스 모델이다.
            Node(
                package="daily_robotics_2026_09_05",
                executable="mock_mobile_base",
                name="mock_mobile_base",
                output="screen",
            ),
            # DriveDistance Goal을 받아 비 RT Action 콜백과 RT 제어 스레드를 연결한다.
            Node(
                package="daily_robotics_2026_09_05",
                executable="drive_action_server",
                name="drive_action_server",
                output="screen",
            ),
            # 서버 발견 후 0.30 m 전진 Goal을 한 번 전송하고 Feedback/Result를 출력한다.
            Node(
                package="daily_robotics_2026_09_05",
                executable="drive_action_client",
                name="drive_action_client",
                output="screen",
            ),
            # 고정된 방 벽을 움직이는 센서 좌표계에서 관측해 /scan으로 게시한다.
            Node(
                package="daily_robotics_2026_09_05",
                executable="scan_simulator",
                name="scan_simulator",
                output="screen",
            ),
            # 연속 스캔 사이의 SE(2) 변환을 고정 8회 point-to-point ICP로 추정한다.
            Node(
                package="daily_robotics_2026_09_05",
                executable="icp_scan_matcher",
                name="icp_scan_matcher",
                output="screen",
            ),
        ]
    )
