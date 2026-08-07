#!/usr/bin/env python3
"""Validate the AI robotics C++ pipeline guide.

이 스크립트는 ROS 설치 없이 문서 구조, 내부 링크, glossary anchor, 코드 주석 밀도를 검사합니다.
"""

# ast는 Python 파일 문법을 실행 없이 검사할 때 사용합니다.
import ast

# re는 Markdown 링크와 heading을 찾기 위해 사용합니다.
import re

# sys는 프로세스 종료 코드를 반환할 때 사용합니다.
import sys

# dataclass는 검사 결과를 담는 작은 클래스를 쉽게 만들게 해줍니다.
from dataclasses import dataclass

# Path는 파일 경로를 객체처럼 다루는 표준 라이브러리입니다.
from pathlib import Path


# @dataclass는 __init__ 같은 기본 메서드를 자동 생성합니다.
@dataclass
class CheckResult:
    """One validation result."""

    # name은 검사 이름입니다.
    name: str

    # passed는 통과 여부입니다.
    passed: bool

    # detail은 실패 원인 또는 통과 설명입니다.
    detail: str


# ROOT는 ai_robotics_cpp_pipeline 폴더입니다.
ROOT = Path(__file__).resolve().parents[1]


# read_text는 UTF-8 텍스트 파일을 읽습니다.
def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


# add_result는 결과 리스트에 새 결과를 추가합니다.
def add_result(results: list[CheckResult], name: str, passed: bool, detail: str) -> None:
    results.append(CheckResult(name=name, passed=passed, detail=detail))


# validate_required_files는 필수 문서와 예제 파일이 존재하는지 검사합니다.
def validate_required_files(results: list[CheckResult]) -> None:
    required_files = [
        "README.md",
        "glossary/README.md",
        "docs/01_ros_ecosystem_theory.md",
        "docs/02_cpp_ml_robot_pipeline.md",
        "docs/03_no_robot_simulation_testing.md",
        "docs/04_llm_agent_robot_apps.md",
        "docs/05_real_robot_testing_pipeline.md",
        "docs/06_embedded_optimization.md",
        "docs/07_daily_lab_curriculum.md",
        "docs/08_validation_report.md",
        "examples/cpp_ml_policy_node/README.md",
        "examples/cpp_ml_policy_node/package.xml",
        "examples/cpp_ml_policy_node/CMakeLists.txt",
        "examples/cpp_ml_policy_node/config/policy_params.yaml",
        "examples/cpp_ml_policy_node/launch/sim_policy_pipeline.launch.py",
        "examples/cpp_ml_policy_node/src/ml_policy_node.cpp",
        "examples/cpp_ml_policy_node/src/safety_filter.cpp",
        "examples/python_agent_bridge/README.md",
        "examples/python_agent_bridge/agent_command_bridge.py",
    ]

    missing = [path for path in required_files if not (ROOT / path).exists()]

    add_result(
        results,
        "required files",
        not missing,
        "missing: " + ", ".join(missing) if missing else "all required files exist",
    )


# validate_markdown_links는 Markdown 상대 링크가 실제 파일을 가리키는지 검사합니다.
def validate_markdown_links(results: list[CheckResult]) -> None:
    link_pattern = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
    broken: list[str] = []

    for markdown_file in ROOT.rglob("*.md"):
        text = read_text(markdown_file)

        for match in link_pattern.finditer(text):
            target = match.group(1)

            if target.startswith("http://") or target.startswith("https://"):
                continue

            if target.startswith("#"):
                continue

            path_part = target.split()[0].split("#")[0]

            if not path_part:
                continue

            linked_path = (markdown_file.parent / path_part).resolve()

            if not linked_path.exists():
                broken.append(f"{markdown_file.relative_to(ROOT)} -> {target}")

    add_result(
        results,
        "markdown links",
        not broken,
        "broken: " + "; ".join(broken) if broken else "all local markdown links resolve",
    )


# extract_glossary_anchors는 glossary heading에서 anchor 이름을 추출합니다.
def extract_glossary_anchors() -> set[str]:
    glossary_text = read_text(ROOT / "glossary/README.md")
    anchors: set[str] = set()

    for line in glossary_text.splitlines():
        if not line.startswith("## "):
            continue

        heading = line.removeprefix("## ").strip().lower()
        anchor = re.sub(r"[^a-z0-9가-힣 -]", "", heading)
        anchor = anchor.replace(" ", "-")
        anchors.add(anchor)

    return anchors


# validate_glossary_links는 glossary로 향하는 링크의 anchor가 실제 heading인지 검사합니다.
def validate_glossary_links(results: list[CheckResult]) -> None:
    anchors = extract_glossary_anchors()
    link_pattern = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
    broken: list[str] = []

    for markdown_file in ROOT.rglob("*.md"):
        text = read_text(markdown_file)

        for match in link_pattern.finditer(text):
            target = match.group(1)

            if "glossary/README.md#" not in target and "../glossary/README.md#" not in target:
                continue

            anchor = target.split("#", 1)[1].strip().lower()

            if anchor not in anchors:
                broken.append(f"{markdown_file.relative_to(ROOT)} -> #{anchor}")

    add_result(
        results,
        "glossary anchors",
        not broken,
        "broken anchors: " + "; ".join(broken) if broken else "all glossary anchors resolve",
    )


# validate_required_glossary_terms는 반드시 사전에 있어야 할 용어를 검사합니다.
def validate_required_glossary_terms(results: list[CheckResult]) -> None:
    anchors = extract_glossary_anchors()
    required_terms = [
        "ros2",
        "node",
        "topic",
        "service",
        "action",
        "tf",
        "rosbag2",
        "ros2-control",
        "rclcpp",
        "cpp",
        "python",
        "pytorch",
        "onnx",
        "onnx-runtime",
        "tensorrt",
        "llm",
        "nlp",
        "llm-agent",
        "langchain",
        "llamaindex",
        "rag",
        "simulator",
        "gazebo",
        "isaac-sim",
        "pybullet",
        "mujoco",
        "safety-filter",
        "dry-run",
        "sim-to-real",
        "embedded-optimization",
        "vla",
    ]

    missing = [term for term in required_terms if term not in anchors]

    add_result(
        results,
        "required glossary terms",
        not missing,
        "missing: " + ", ".join(missing) if missing else "all required terms exist",
    )


# validate_keyword_coverage는 요청된 기술 키워드가 문서에 포함되는지 검사합니다.
def validate_keyword_coverage(results: list[CheckResult]) -> None:
    combined_text = "\n".join(read_text(path) for path in ROOT.rglob("*.md")).lower()

    required_keywords = [
        "c++",
        "python",
        "ros 2",
        "llm",
        "nlp",
        "langchain",
        "llamaindex",
        "vla",
        "isaac sim",
        "pybullet",
        "mujoco",
        "onnx runtime",
        "tensorrt",
        "dry-run",
        "closed-loop",
        "sim-to-real",
        "safety filter",
    ]

    missing = [keyword for keyword in required_keywords if keyword not in combined_text]

    add_result(
        results,
        "keyword coverage",
        not missing,
        "missing: " + ", ".join(missing) if missing else "all requested topics covered",
    )


# validate_python_syntax는 Python 예제 문법을 검사합니다.
def validate_python_syntax(results: list[CheckResult]) -> None:
    failures: list[str] = []

    for python_file in ROOT.rglob("*.py"):
        source = read_text(python_file)

        try:
            ast.parse(source, filename=str(python_file))
        except SyntaxError as error:
            failures.append(f"{python_file.relative_to(ROOT)}: {error}")

    add_result(
        results,
        "python syntax",
        not failures,
        "failures: " + "; ".join(failures) if failures else "all Python examples parse",
    )


# validate_cpp_comment_density는 C++ 코드가 초보자용 주석을 충분히 갖는지 검사합니다.
def validate_cpp_comment_density(results: list[CheckResult]) -> None:
    failures: list[str] = []

    for cpp_file in ROOT.rglob("*.cpp"):
        lines = read_text(cpp_file).splitlines()
        non_empty = [line for line in lines if line.strip()]
        comment_lines = [line for line in non_empty if line.strip().startswith("//")]
        ratio = len(comment_lines) / max(len(non_empty), 1)

        if ratio < 0.35:
            failures.append(f"{cpp_file.relative_to(ROOT)} comment ratio {ratio:.2f}")

    add_result(
        results,
        "cpp comment density",
        not failures,
        "failures: " + "; ".join(failures) if failures else "C++ examples are heavily commented",
    )


# run_all_checks는 모든 검사를 순서대로 실행합니다.
def run_all_checks() -> list[CheckResult]:
    results: list[CheckResult] = []
    validate_required_files(results)
    validate_markdown_links(results)
    validate_glossary_links(results)
    validate_required_glossary_terms(results)
    validate_keyword_coverage(results)
    validate_python_syntax(results)
    validate_cpp_comment_density(results)
    return results


# print_results는 검사 결과를 터미널에 출력합니다.
def print_results(results: list[CheckResult]) -> None:
    for result in results:
        status = "PASS" if result.passed else "FAIL"
        print(f"[{status}] {result.name}: {result.detail}")


# main은 스크립트의 실행 진입점입니다.
def main() -> int:
    results = run_all_checks()
    print_results(results)
    return 0 if all(result.passed for result in results) else 1


# 직접 실행될 때 main 반환값으로 종료합니다.
if __name__ == "__main__":
    sys.exit(main())
