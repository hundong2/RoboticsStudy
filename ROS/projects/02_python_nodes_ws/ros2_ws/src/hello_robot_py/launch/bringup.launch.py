"""Launch two learning nodes together.

launch 파일은 여러 노드를 한 번에 실행하기 위한 Python 파일입니다.
ROS 2 launch는 Python 문법을 사용하지만, 반환값은 LaunchDescription 객체입니다.
"""

# LaunchDescription은 실행할 노드와 설정 목록을 담는 컨테이너입니다.
from launch import LaunchDescription

# Node action은 launch 시스템이 ROS 노드를 실행하도록 지시합니다.
from launch_ros.actions import Node


# generate_launch_description 함수 이름은 ROS 2 launch가 찾는 약속된 이름입니다.
def generate_launch_description():
    # LaunchDescription 안에 실행할 Node들을 리스트로 넣습니다.
    return LaunchDescription(
        [
            # 첫 번째 Node는 hello_publisher 실행 파일을 시작합니다.
            Node(
                # package는 실행 파일이 들어 있는 ROS 패키지 이름입니다.
                package="hello_robot_py",
                # executable은 setup.py entry_points에 등록한 실행 이름입니다.
                executable="hello_publisher",
                # name은 ROS 그래프에서 보일 노드 이름을 덮어쓸 수 있습니다.
                name="hello_publisher",
                # parameters는 노드 parameter 초기값을 설정합니다.
                parameters=[{"robot_name": "launch_bot"}],
                # output="screen"은 로그를 터미널 화면에 보여줍니다.
                output="screen",
            ),
            # 두 번째 Node는 hello_subscriber 실행 파일을 시작합니다.
            Node(
                package="hello_robot_py",
                executable="hello_subscriber",
                name="hello_subscriber",
                output="screen",
            ),
        ]
    )
