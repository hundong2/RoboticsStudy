#!/usr/bin/env python3
"""Validate the ROS learning materials without requiring ROS to be installed.

이 스크립트는 학습 자료의 구조, 내부 링크, Python 문법, 코드 주석 밀도를 검사합니다.
ROS 2 런타임이 없어도 실행 가능해야 하므로 rclpy를 import하지 않습니다.
"""

# ast는 Python 코드를 실행하지 않고 문법 트리로 파싱하는 표준 라이브러리입니다.
import ast

# re는 정규표현식으로 문자열 패턴을 찾는 표준 라이브러리입니다.
import re

# sys는 프로그램 종료 코드 같은 인터프리터 기능을 다룰 때 사용합니다.
import sys

# dataclass는 단순한 데이터 묶음 클래스를 쉽게 만들게 해줍니다.
from dataclasses import dataclass

# Path는 파일 경로를 객체처럼 다루는 표준 라이브러리 클래스입니다.
from pathlib import Path


# @dataclass는 아래 클래스에 __init__ 같은 기본 메서드를 자동 생성합니다.
@dataclass
class CheckResult:
    """Store one validation check result."""

    # name은 검사 이름입니다.
    name: str

    # passed는 검사 통과 여부입니다.
    passed: bool

    # detail은 실패 원인 또는 참고 설명입니다.
    detail: str


# ROOT는 이 스크립트 위치를 기준으로 ROS 폴더를 계산합니다.
# __file__은 현재 파일 경로이고, parents[1]은 ROS 폴더입니다.
ROOT = Path(__file__).resolve().parents[1]

# REPO_ROOT는 프로젝트 최상단 폴더입니다.
REPO_ROOT = ROOT.parent


# add_result 함수는 검사 결과 리스트에 CheckResult를 추가합니다.
def add_result(results, name, passed, detail):
    # append는 Python 리스트 끝에 새 값을 추가하는 메서드입니다.
    results.append(CheckResult(name=name, passed=passed, detail=detail))


# read_text 함수는 UTF-8 텍스트 파일을 읽습니다.
def read_text(path):
    # Path.read_text는 파일 전체를 문자열로 읽습니다.
    return path.read_text(encoding="utf-8")


# validate_required_files는 학습 자료가 갖춰야 할 핵심 파일 존재 여부를 확인합니다.
def validate_required_files(results):
    # 리스트 안에는 ROS 폴더 기준 상대 경로를 문자열로 넣습니다.
    required_files = [
        "README.md",
        "docs/00_environment_setup.md",
        "docs/01_ros_concepts.md",
        "docs/02_cli_graph_debugging.md",
        "docs/03_python_package_nodes.md",
        "docs/04_messages_services_actions.md",
        "docs/05_tf2_urdf_robot_modeling.md",
        "docs/06_gazebo_simulation.md",
        "docs/07_nav2_slam_localization.md",
        "docs/08_moveit2_manipulation.md",
        "docs/09_perception_ai_edge.md",
        "docs/10_security_realtime_operations.md",
        "docs/11_latest_trends_2026.md",
        "docs/12_capstone_assessment.md",
        "docs/13_simulation_without_robot.md",
        "docs/validation_plan.md",
        "docs/validation_report.md",
        "projects/README.md",
        "projects/01_turtlesim_cli_lab/README.md",
        "projects/02_python_nodes_ws/README.md",
        "projects/03_tf_urdf_robot_model/README.md",
        "projects/04_nav2_simulation_mission/README.md",
        "projects/05_capstone_autonomous_inspection/README.md",
        "projects/06_robotless_simulation_lab/README.md",
    ]

    # missing 리스트에는 존재하지 않는 파일 경로를 모읍니다.
    missing = []

    # for 문은 리스트의 각 항목을 하나씩 반복합니다.
    for relative_path in required_files:
        # / 연산자는 Path 객체에서 하위 경로를 만들 때 사용됩니다.
        if not (ROOT / relative_path).exists():
            missing.append(relative_path)

    # len(missing) == 0이면 빠진 파일이 없다는 뜻입니다.
    add_result(
        results,
        "required files",
        len(missing) == 0,
        "missing: " + ", ".join(missing) if missing else "all required files exist",
    )


# validate_markdown_links는 Markdown 내부 상대 링크가 실제 파일을 가리키는지 검사합니다.
def validate_markdown_links(results):
    # rglob("*.md")는 ROS 폴더 아래 모든 Markdown 파일을 재귀적으로 찾습니다.
    markdown_files = list(ROOT.rglob("*.md"))

    # broken 리스트에는 깨진 링크 설명을 모읍니다.
    broken = []

    # Markdown 링크 패턴입니다. [label](target) 형태를 찾습니다.
    link_pattern = re.compile(r"\[[^\]]+\]\(([^)]+)\)")

    for markdown_file in markdown_files:
        text = read_text(markdown_file)
        for match in link_pattern.finditer(text):
            target = match.group(1)

            # http 링크는 외부 링크이므로 파일 존재 검사에서 제외합니다.
            if target.startswith("http://") or target.startswith("https://"):
                continue

            # anchor 링크는 현재 문서 내부 위치이므로 제외합니다.
            if target.startswith("#"):
                continue

            # 공백 뒤 title이 붙은 Markdown 링크는 첫 토큰만 경로로 봅니다.
            path_part = target.split()[0]

            # fragment(#section)가 붙은 경우 파일 경로만 검사합니다.
            path_part = path_part.split("#")[0]

            # 빈 경로는 검사할 파일이 없습니다.
            if not path_part:
                continue

            # 상대 링크는 링크가 들어 있는 Markdown 파일의 폴더를 기준으로 해석합니다.
            linked_path = (markdown_file.parent / path_part).resolve()

            if not linked_path.exists():
                relative_markdown = markdown_file.relative_to(ROOT)
                broken.append(f"{relative_markdown} -> {target}")

    add_result(
        results,
        "markdown links",
        len(broken) == 0,
        "broken: " + "; ".join(broken) if broken else "all local markdown links resolve",
    )


# validate_root_readme_link는 프로젝트 최상단 README에 ROS 링크가 있는지 검사합니다.
def validate_root_readme_link(results):
    root_readme = REPO_ROOT / "README.md"

    if not root_readme.exists():
        add_result(results, "root README link", False, "README.md not found")
        return

    text = read_text(root_readme)
    has_link = "./ROS/README.md" in text or "ROS/README.md" in text

    add_result(
        results,
        "root README link",
        has_link,
        "root README links to ROS/README.md" if has_link else "missing ROS README link",
    )


# validate_curriculum_keywords는 입문부터 전문가까지 핵심 키워드가 모두 등장하는지 검사합니다.
def validate_curriculum_keywords(results):
    # 모든 Markdown 내용을 하나의 큰 문자열로 합칩니다.
    combined_text = "\n".join(read_text(path) for path in ROOT.rglob("*.md"))

    # lower는 영문 대소문자 차이를 줄이기 위해 모두 소문자로 바꿉니다.
    lowered = combined_text.lower()

    required_keywords = [
        "node",
        "topic",
        "service",
        "action",
        "parameter",
        "tf",
        "urdf",
        "gazebo",
        "nav2",
        "moveit2",
        "rosbag2",
        "lyrical",
        "jazzy",
        "kilted",
        "zenoh",
        "zero-copy",
        "physical ai",
        "webots",
        "isaac sim",
        "mujoco",
        "pybullet",
        "drake",
        "coppeliasim",
        "carla",
    ]

    missing = [keyword for keyword in required_keywords if keyword not in lowered]

    add_result(
        results,
        "curriculum keywords",
        len(missing) == 0,
        "missing: " + ", ".join(missing) if missing else "all key topics covered",
    )


# validate_python_syntax는 예제 Python 파일이 문법적으로 올바른지 확인합니다.
def validate_python_syntax(results):
    python_files = list(ROOT.rglob("*.py"))
    failures = []

    for python_file in python_files:
        source = read_text(python_file)
        try:
            # ast.parse는 코드를 실행하지 않고 문법만 분석합니다.
            ast.parse(source, filename=str(python_file))
        except SyntaxError as error:
            relative_path = python_file.relative_to(ROOT)
            failures.append(f"{relative_path}: {error}")

    add_result(
        results,
        "python syntax",
        len(failures) == 0,
        "failures: " + "; ".join(failures) if failures else "all Python files parse",
    )


# validate_comment_density는 학습용 코드가 충분히 설명적인지 대략 검사합니다.
def validate_comment_density(results):
    # ROS 노드 예제만 주석 밀도를 검사합니다.
    code_dir = ROOT / "projects/02_python_nodes_ws/ros2_ws/src/hello_robot_py"
    python_files = list(code_dir.rglob("*.py"))

    failures = []

    for python_file in python_files:
        lines = read_text(python_file).splitlines()

        # 빈 줄은 주석 밀도 계산에서 제외합니다.
        non_empty_lines = [line for line in lines if line.strip()]

        # 주석 줄 또는 docstring으로 보이는 줄을 설명 줄로 계산합니다.
        comment_lines = [
            line
            for line in non_empty_lines
            if line.strip().startswith("#")
            or line.strip().startswith('"""')
            or line.strip().endswith('"""')
        ]

        # max(..., 1)은 0으로 나누는 오류를 막습니다.
        ratio = len(comment_lines) / max(len(non_empty_lines), 1)

        # 초보 학습용 코드이므로 설명 줄 비율을 30% 이상으로 요구합니다.
        if ratio < 0.30:
            relative_path = python_file.relative_to(ROOT)
            failures.append(f"{relative_path}: comment ratio {ratio:.2f}")

    add_result(
        results,
        "code comments",
        len(failures) == 0,
        "failures: " + "; ".join(failures)
        if failures
        else "learning code has dense beginner comments",
    )


# run_all_checks는 모든 검사 함수를 순서대로 실행합니다.
def run_all_checks():
    results = []

    validate_required_files(results)
    validate_markdown_links(results)
    validate_root_readme_link(results)
    validate_curriculum_keywords(results)
    validate_python_syntax(results)
    validate_comment_density(results)

    return results


# print_results는 사람이 읽기 쉬운 형태로 결과를 출력합니다.
def print_results(results):
    for result in results:
        status = "PASS" if result.passed else "FAIL"
        print(f"[{status}] {result.name}: {result.detail}")


# main은 스크립트 실행 진입점입니다.
def main():
    results = run_all_checks()
    print_results(results)

    # all 함수는 모든 항목이 True인지 확인합니다.
    success = all(result.passed for result in results)

    # 성공이면 0, 실패면 1을 반환합니다. CI에서 이 종료 코드를 사용할 수 있습니다.
    return 0 if success else 1


# 이 파일을 직접 실행하면 main의 반환값으로 프로세스를 종료합니다.
if __name__ == "__main__":
    sys.exit(main())
