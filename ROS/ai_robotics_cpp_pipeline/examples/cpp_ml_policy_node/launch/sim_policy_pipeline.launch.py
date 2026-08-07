"""Launch the C++ ML policy node and safety filter together.

이 launch 파일은 시뮬레이터 또는 rosbag2가 /scan을 제공한다는 가정으로 작성했습니다.
"""

# LaunchDescription은 launch 시스템이 실행할 action 목록을 담습니다.
from launch import LaunchDescription

# Node action은 ROS 2 노드를 실행하는 launch action입니다.
from launch_ros.actions import Node


# ROS 2 launch는 이 이름의 함수를 찾아 launch 설정을 얻습니다.
def generate_launch_description():
    # LaunchDescription 객체를 반환하면 launch 시스템이 내부 Node들을 실행합니다.
    return LaunchDescription(
        [
            # ml_policy_node는 센서 입력을 읽고 원시 정책 명령을 publish합니다.
            Node(
                package="cpp_ml_policy_node",
                executable="ml_policy_node",
                name="ml_policy_node",
                output="screen",
                parameters=["config/policy_params.yaml"],
            ),
            # safety_filter는 원시 명령을 안전하게 제한한 뒤 /cmd_vel로 publish합니다.
            Node(
                package="cpp_ml_policy_node",
                executable="safety_filter",
                name="safety_filter",
                output="screen",
                parameters=["config/policy_params.yaml"],
            ),
        ]
    )
