"""세 보안 enclave에 대응하는 pose-graph SLAM 학습 노드를 실행한다."""

from launch import LaunchDescription
# launch_ros.actions.Node는 패키지 실행 파일, 이름, remapping, ROS 인자를 선언한다.
from launch_ros.actions import Node


def generate_launch_description():
    """각 프로세스에 서로 다른 SROS2 enclave 이름을 부여해 최소 권한 경계를 드러낸다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_11",
                executable="path_simulator",
                name="path_simulator",
                output="screen",
                # --enclave는 보안이 켜졌을 때 사용할 인증서/권한 묶음의 논리 이름이다.
                # 보안이 꺼진 일반 smoke test에서도 노드 이름과 별개인 경계를 확인할 수 있다.
                ros_arguments=["--enclave", "/daily_slam/simulator"],
            ),
            Node(
                package="daily_robotics_2026_09_11",
                executable="bounded_pose_graph_optimizer",
                name="pose_graph_optimizer",
                output="screen",
                ros_arguments=["--enclave", "/daily_slam/optimizer"],
            ),
            Node(
                package="daily_robotics_2026_09_11",
                executable="path_auditor",
                name="path_auditor",
                output="screen",
                ros_arguments=["--enclave", "/daily_slam/auditor"],
            ),
        ]
    )
