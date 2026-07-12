"""Install configuration for the hello_robot_py ROS 2 package.

이 파일은 Python 패키지를 ROS 2가 실행할 수 있는 형태로 설치하는 규칙입니다.
초보 학습을 위해 일반 Python 문법과 ROS 2 패키징 의미를 함께 주석으로 설명합니다.
"""

# glob은 파일 이름 패턴을 이용해 여러 파일 경로를 찾는 표준 라이브러리입니다.
from glob import glob

# os.path.join은 운영체제에 맞는 경로 구분자를 사용해 경로를 합칩니다.
import os

# setuptools.setup은 Python 패키지 설치 규칙을 선언하는 함수입니다.
from setuptools import setup

# package_name 변수에는 ROS 패키지 이름을 문자열로 저장합니다.
# 같은 이름이 여러 곳에 반복되므로 변수로 두면 오타를 줄일 수 있습니다.
package_name = "hello_robot_py"

# setup 함수 호출은 "이 패키지를 어떻게 설치할 것인가"를 Python에게 알려줍니다.
setup(
    # name은 설치될 Python/ROS 패키지 이름입니다.
    name=package_name,
    # version은 패키지 버전입니다. package.xml의 version과 맞추는 것이 좋습니다.
    version="0.1.0",
    # packages는 설치할 Python 모듈 폴더 목록입니다.
    packages=[package_name],
    # data_files는 Python 코드가 아닌 package.xml, launch 파일 같은 자료를 설치합니다.
    data_files=[
        # ament index에 패키지 존재를 알리는 marker 파일을 설치합니다.
        (
            "share/ament_index/resource_index/packages",
            ["resource/" + package_name],
        ),
        # package.xml은 ROS 도구가 패키지 메타데이터를 읽는 데 필요합니다.
        ("share/" + package_name, ["package.xml"]),
        # launch 폴더의 모든 *.launch.py 파일을 share/<package>/launch에 설치합니다.
        (
            os.path.join("share", package_name, "launch"),
            glob(os.path.join("launch", "*.launch.py")),
        ),
    ],
    # install_requires는 일반 Python 패키지 의존성을 적는 곳입니다.
    install_requires=["setuptools"],
    # zip_safe=False는 ROS 2에서 파일 경로 접근이 단순해지도록 압축 설치를 피합니다.
    zip_safe=False,
    # maintainer는 패키지 관리자 이름입니다.
    maintainer="RoboticsStudy",
    # maintainer_email은 패키지 관리자 이메일입니다.
    maintainer_email="study@example.com",
    # description은 패키지 설명입니다.
    description="Comment-heavy ROS 2 Python examples for beginner ROS learners.",
    # license는 사용 조건입니다.
    license="MIT",
    # tests_require는 오래된 setuptools 테스트 설정입니다. 학습용으로 비워 둡니다.
    tests_require=[],
    # entry_points는 콘솔 명령 이름을 Python 함수와 연결합니다.
    entry_points={
        # console_scripts 항목은 ros2 run으로 실행할 수 있는 명령을 만듭니다.
        "console_scripts": [
            # 왼쪽 hello_publisher는 실행 이름입니다.
            # 오른쪽 hello_robot_py.hello_publisher:main은 모듈과 함수 위치입니다.
            "hello_publisher = hello_robot_py.hello_publisher:main",
            "hello_subscriber = hello_robot_py.hello_subscriber:main",
            "parameter_timer = hello_robot_py.parameter_timer:main",
        ],
    },
)
