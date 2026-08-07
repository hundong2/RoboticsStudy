#!/usr/bin/env python3
"""Convert a natural-language robot instruction into a safe JSON plan.

이 파일은 LangChain/LlamaIndex 같은 실제 agent framework를 붙이기 전,
"LLM 출력 -> 구조화된 계획 -> 안전 검증" 흐름을 초보자가 이해하도록 만든 예제입니다.
"""

# argparse는 명령줄 인자를 읽기 위한 Python 표준 라이브러리입니다.
import argparse

# json은 Python 객체를 JSON 문자열로 바꾸는 표준 라이브러리입니다.
import json

# dataclass는 단순한 데이터 클래스를 쉽게 만들게 해줍니다.
from dataclasses import dataclass

# asdict는 dataclass 객체를 dict로 변환합니다.
from dataclasses import asdict


# @dataclass는 __init__ 같은 기본 메서드를 자동 생성합니다.
@dataclass
class RobotPlan:
    """A small validated plan that can later be mapped to ROS 2 actions."""

    # goal은 로봇이 수행할 추상 목표입니다.
    goal: str

    # mode는 실행 모드입니다. 예시는 dry_run, simulation, real_robot입니다.
    mode: str

    # max_speed_mps는 최대 속도 제한입니다.
    max_speed_mps: float

    # require_human_approval은 실제 로봇 실행 전 사람 승인이 필요한지 나타냅니다.
    require_human_approval: bool

    # notes는 agent가 남긴 설명입니다.
    notes: str


# build_plan_from_text는 자연어를 안전한 계획 객체로 바꿉니다.
def build_plan_from_text(text: str) -> RobotPlan:
    # lower는 대소문자를 통일해 키워드 검색을 쉽게 합니다.
    lowered = text.lower()

    # 기본 목표는 사용자가 입력한 문장 자체로 둡니다.
    goal = text.strip()

    # 기본 모드는 dry_run입니다. 실제 로봇으로 바로 가지 않는 것이 안전합니다.
    mode = "dry_run"

    # 기본 속도는 매우 낮게 둡니다.
    max_speed_mps = 0.10

    # 실제 로봇 관련 단어가 있으면 사람 승인을 강제합니다.
    require_human_approval = True

    # "simulation" 또는 "시뮬레이션"이 있으면 simulation 모드로 둡니다.
    if "simulation" in lowered or "시뮬레이션" in text:
        mode = "simulation"

    # "빠르게" 같은 단어가 있어도 안전 제한 때문에 속도를 크게 올리지 않습니다.
    if "빠르게" in text or "fast" in lowered:
        max_speed_mps = 0.15

    # 위험 단어가 있으면 목표는 유지하되 실제 실행 금지 메모를 남깁니다.
    dangerous = any(word in lowered for word in ["ignore safety", "disable stop", "bypass"])

    # notes에는 검증 이유를 사람이 읽을 수 있게 적습니다.
    notes = "Generated as a safe structured plan; direct motor commands are not allowed."

    # 위험 단어가 발견되면 notes를 더 강하게 바꿉니다.
    if dangerous:
        mode = "dry_run"
        max_speed_mps = 0.0
        notes = "Dangerous instruction detected; execution blocked."

    # RobotPlan dataclass 객체를 반환합니다.
    return RobotPlan(
        goal=goal,
        mode=mode,
        max_speed_mps=max_speed_mps,
        require_human_approval=require_human_approval,
        notes=notes,
    )


# validate_plan은 계획이 최소 안전 규칙을 만족하는지 검사합니다.
def validate_plan(plan: RobotPlan) -> list[str]:
    # errors 리스트에는 발견한 문제를 문자열로 모읍니다.
    errors: list[str] = []

    # 실제 로봇 모드에서 사람 승인이 없으면 오류입니다.
    if plan.mode == "real_robot" and not plan.require_human_approval:
        errors.append("real_robot mode requires human approval")

    # 속도 제한이 너무 크면 오류입니다.
    if plan.max_speed_mps > 0.20:
        errors.append("max_speed_mps is too high for first-stage testing")

    # 목표가 비어 있으면 오류입니다.
    if not plan.goal:
        errors.append("goal is empty")

    # 오류 리스트를 반환합니다. 빈 리스트면 통과입니다.
    return errors


# main은 명령줄 실행 진입점입니다.
def main() -> int:
    # ArgumentParser는 CLI 인자 파서를 만듭니다.
    parser = argparse.ArgumentParser()

    # instruction 인자는 사용자의 자연어 명령입니다.
    parser.add_argument("instruction")

    # parse_args는 실제 명령줄 인자를 읽습니다.
    args = parser.parse_args()

    # 자연어 명령을 구조화된 계획으로 바꿉니다.
    plan = build_plan_from_text(args.instruction)

    # 계획을 안전 규칙으로 검증합니다.
    errors = validate_plan(plan)

    # asdict는 dataclass를 JSON 직렬화 가능한 dict로 바꿉니다.
    payload = asdict(plan)

    # validation_errors 필드를 추가합니다.
    payload["validation_errors"] = errors

    # json.dumps는 dict를 사람이 읽기 쉬운 JSON 문자열로 바꿉니다.
    print(json.dumps(payload, ensure_ascii=False, indent=2))

    # 오류가 있으면 종료 코드 1, 없으면 0을 반환합니다.
    return 1 if errors else 0


# 이 파일을 직접 실행했을 때만 main을 호출합니다.
if __name__ == "__main__":
    raise SystemExit(main())
