"""
VLM weekly practice project.

2026-07-05 ~ 2026-07-10 리서치 노트의 핵심 아이디어를 작은 코드로 실습한다.
실제 VLM, diffusion model, event camera, UE5 환경은 사용하지 않는다.
대신 각 논문의 "왜 필요한가"를 숫자와 toy loop로 확인한다.

실행:
    python Research\\2026-07-10_VLM_Weekly_Synthesis_Practice\\weekly_vlm_practice_project\\weekly_vlm_practice.py
"""

from __future__ import annotations

from dataclasses import dataclass
from math import hypot
from random import Random


def rswa_cache_demo() -> None:
    """Unlimited OCR의 R-SWA 직관: generated token cache를 window로 제한한다."""

    reference_tokens = 1024
    window = 128
    lengths = [512, 2048, 8192, 32768]

    print("1) R-SWA cache simulator")
    print(f"{'generated':>10} | {'full tokens':>11} | {'r-swa tokens':>12} | {'saving':>8}")
    print("-" * 58)
    for generated in lengths:
        full = reference_tokens + generated
        rswa = reference_tokens + min(generated, window)
        saving = 1 - rswa / full
        print(f"{generated:>10} | {full:>11} | {rswa:>12} | {saving:>7.1%}")
    print("why: 긴 문서 OCR에서 출력이 길어져도 메모리 상한을 유지하기 위해 필요하다.\n")


@dataclass
class Question:
    text: str
    difficulty: int
    visual_grounding: int


def self_evolving_questioner_demo() -> None:
    """Self-Evolving Visual Questioner의 proposer/filter loop를 흉내 낸다."""

    rng = Random(3)
    target_min_difficulty = 3
    accepted: list[Question] = []

    print("2) Self-evolving visual questioner toy loop")
    for round_id in range(1, 6):
        proposed = Question(
            text=f"Round {round_id}: What spatial/causal detail is missing in this image?",
            difficulty=round_id + rng.choice([0, 1]),
            visual_grounding=rng.choice([1, 2, 3, 4]),
        )
        keep = proposed.difficulty >= target_min_difficulty and proposed.visual_grounding >= 3
        print(
            f"round={round_id} difficulty={proposed.difficulty} "
            f"grounding={proposed.visual_grounding} keep={keep}"
        )
        if keep:
            accepted.append(proposed)

    print(f"accepted questions: {len(accepted)}")
    print("why: 사람이 계속 질문을 만들지 않아도 모델 약점을 찌르는 curriculum을 만들기 위해 필요하다.\n")


@dataclass(frozen=True)
class ObjectBox:
    name: str
    x1: int
    y1: int
    x2: int
    y2: int


def gavel_style_demo() -> None:
    """GAVEL 직관: caption claim을 object grounding과 대조한다."""

    objects = {
        "red car": ObjectBox("red car", 10, 20, 90, 80),
        "blue bus": ObjectBox("blue bus", 120, 25, 220, 95),
        "traffic light": ObjectBox("traffic light", 240, 5, 265, 55),
    }
    caption_claims = ["red car exists", "green bicycle exists", "traffic light exists"]

    print("3) GAVEL-style caption verification/localization")
    for claim in caption_claims:
        subject = claim.replace(" exists", "")
        if subject in objects:
            box = objects[subject]
            print(f"OK    | {claim} | evidence bbox=({box.x1},{box.y1},{box.x2},{box.y2})")
        else:
            print(f"ERROR | {claim} | no visual evidence found")
    print("why: 환각을 줄이려면 오류 claim과 시각 증거 위치를 함께 찾아야 한다.\n")


def selective_gate_demo() -> None:
    """Risk-aware selective inference: fast model을 언제 믿고 언제 보류할지 결정한다."""

    samples = [
        {"name": "normal attentive", "confidence": 0.92, "risk": 0.05},
        {"name": "glance away", "confidence": 0.61, "risk": 0.45},
        {"name": "drowsy high risk", "confidence": 0.74, "risk": 0.82},
        {"name": "sensor noisy", "confidence": 0.43, "risk": 0.40},
    ]

    print("4) Risk-aware selective inference gate")
    for sample in samples:
        if sample["risk"] > 0.7:
            action = "slow_verify"
        elif sample["confidence"] < 0.55:
            action = "abstain_warn"
        else:
            action = "accept_fast"
        print(f"{sample['name']:<18} confidence={sample['confidence']:.2f} risk={sample['risk']:.2f} -> {action}")
    print("why: 항상 큰 모델을 쓰지 않으면서 위험한 false negative를 줄이기 위해 필요하다.\n")


def bev_risk_demo() -> None:
    """DriveMRP 직관: BEV에서 ego trajectory와 obstacle distance로 risk를 계산한다."""

    ego_path = [(0, 0), (2, 0), (4, 0), (6, 0), (8, 0)]
    obstacles = [(5, 0.5), (8, 4), (3, -3)]

    min_distance = min(hypot(ex - ox, ey - oy) for ex, ey in ego_path for ox, oy in obstacles)
    risk = max(0.0, 1.0 - min_distance / 5.0)

    print("5) BEV high-risk motion scoring")
    print(f"min distance to obstacle: {min_distance:.2f}")
    print(f"risk score              : {risk:.2f}")
    print("why: 실제 사고 직전 데이터는 희귀하므로 BEV 시뮬레이션으로 위험 상황을 보강해야 한다.\n")


def world_model_demo() -> None:
    """Multiplayer world model 직관: 여러 agent의 action stream으로 미래 상태를 rollout한다."""

    state = {"ball": [0, 0], "p1": [-2, 0], "p2": [2, 0]}
    actions = [
        {"p1": (1, 0), "p2": (-1, 0)},
        {"p1": (1, 0), "p2": (-1, 0)},
        {"p1": (0, 1), "p2": (0, -1)},
    ]

    print("6) Multi-agent world rollout")
    for step, action in enumerate(actions, start=1):
        for player, delta in action.items():
            state[player][0] += delta[0]
            state[player][1] += delta[1]
        # 공은 두 플레이어의 평균 이동 방향을 약하게 따라간다고 가정한다.
        avg_dx = sum(delta[0] for delta in action.values()) / len(action)
        avg_dy = sum(delta[1] for delta in action.values()) / len(action)
        state["ball"][0] += avg_dx
        state["ball"][1] += avg_dy
        print(f"step={step} state={state}")
    print("why: 다중 agent 환경에서는 각 agent 행동이 미래 장면에 미치는 영향을 분리해 예측해야 한다.\n")


def main() -> None:
    rswa_cache_demo()
    self_evolving_questioner_demo()
    gavel_style_demo()
    selective_gate_demo()
    bev_risk_demo()
    world_model_demo()


if __name__ == "__main__":
    main()
