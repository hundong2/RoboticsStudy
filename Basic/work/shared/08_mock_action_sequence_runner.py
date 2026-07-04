"""
Mock robot action sequence runner.

Why this matters:
    LLM/Ollama should produce a constrained plan, not raw motor commands. This
    runner demonstrates how to validate an action sequence before execution.

Run:
    python shared/08_mock_action_sequence_runner.py
"""

from __future__ import annotations

from dataclasses import dataclass


ALLOWED_ACTIONS = {"detect_object", "navigate_to", "pick", "place", "stop"}


@dataclass
class Step:
    action: str
    target: str


def validate_plan(plan: list[Step]) -> tuple[bool, str]:
    for index, step in enumerate(plan, start=1):
        if step.action not in ALLOWED_ACTIONS:
            return False, f"step {index}: action not allowed: {step.action}"
        if step.action in {"pick", "place", "navigate_to", "detect_object"} and not step.target:
            return False, f"step {index}: target is required"
    return True, "ok"


def execute_mock(step: Step) -> None:
    if step.action == "detect_object":
        print(f"[vision] detect target={step.target}")
    elif step.action == "navigate_to":
        print(f"[nav2] send goal target={step.target}")
    elif step.action == "pick":
        print(f"[moveit2] plan and execute pick target={step.target}")
    elif step.action == "place":
        print(f"[moveit2] plan and execute place target={step.target}")
    elif step.action == "stop":
        print("[safety] stop")


def main() -> None:
    plan = [
        Step("detect_object", "red cup"),
        Step("pick", "red cup"),
        Step("place", "left box"),
    ]

    ok, reason = validate_plan(plan)
    print("validation:", ok, reason)
    if not ok:
        return

    for step in plan:
        execute_mock(step)

    print("\nNext step")
    print("- Replace each mock print with a ROS 2 service/action client.")
    print("- Keep validation before execution.")
    print("- Add timeout and failure recovery for every action.")


if __name__ == "__main__":
    main()
