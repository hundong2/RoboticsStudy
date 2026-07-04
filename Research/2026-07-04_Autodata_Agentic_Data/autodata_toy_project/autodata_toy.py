"""
Autodata toy implementation.

이 코드는 논문 "Autodata: An agentic data scientist to create high quality
synthetic data"의 실제 구현이 아니다. LLM API도 사용하지 않는다.
대신 Agentic Self-Instruct의 핵심 루프를 표준 라이브러리만으로 재현한다.

핵심 루프:
    1. challenger가 문제를 만든다.
    2. weak solver와 strong solver가 문제를 푼다고 가정하고 점수를 낸다.
    3. judge가 strong-weak gap과 품질을 평가한다.
    4. 실패하면 feedback을 바탕으로 recipe를 수정한다.
    5. 적절한 난이도 문제만 accepted_examples.jsonl에 저장한다.

실행:
    python Research\\2026-07-04_Autodata_Agentic_Data\\autodata_toy_project\\autodata_toy.py
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path
from random import Random


ROOT = Path(__file__).resolve().parent
SOURCES_PATH = ROOT / "sample_sources.json"
OUTPUT_PATH = ROOT / "accepted_examples.jsonl"


@dataclass
class Source:
    """데이터 생성을 위한 원천 지식."""

    topic: str
    easy_angle: str
    hard_angle: str


@dataclass
class Recipe:
    """challenger가 문제를 생성할 때 따르는 간단한 recipe."""

    specificity: int = 1
    difficulty: int = 1
    require_reasoning_steps: bool = False


@dataclass
class CandidateExample:
    """생성된 synthetic example."""

    topic: str
    question: str
    reference_answer: str
    rubric: list[str]
    difficulty: int
    specificity: int


@dataclass
class JudgeReport:
    """judge가 candidate를 평가한 결과."""

    accepted: bool
    weak_score: float
    strong_score: float
    gap: float
    reason: str
    suggestion: str


def load_sources() -> list[Source]:
    """JSON source 파일을 읽는다."""

    data = json.loads(SOURCES_PATH.read_text(encoding="utf-8"))
    return [Source(**item) for item in data]


def challenger(source: Source, recipe: Recipe, rng: Random) -> CandidateExample:
    """문제 생성기.

    실제 논문에서는 LLM challenger가 source document를 읽고 문제/정답/루브릭을
    생성한다. 여기서는 recipe 값에 따라 질문의 구체성과 난이도를 바꾼다.
    """

    if recipe.difficulty <= 1:
        angle = source.easy_angle
    else:
        angle = source.hard_angle

    if recipe.require_reasoning_steps:
        reasoning_hint = " Explain the key reasoning steps and failure mode."
    else:
        reasoning_hint = ""

    question = (
        f"Topic: {source.topic}. {angle}"
        f" Specificity level={recipe.specificity}, difficulty level={recipe.difficulty}."
        f"{reasoning_hint}"
    )

    reference_answer = (
        f"A strong answer should address '{source.topic}' using the requested angle, "
        "state the core mechanism, and mention at least one common pitfall."
    )

    rubric = [
        "Correctly identifies the core mechanism.",
        "Explains why the easy intuition is incomplete.",
        "Mentions a concrete failure mode or edge case.",
    ]

    if recipe.specificity >= 3:
        rubric.append("Uses the exact terminology from the source topic.")
    if recipe.require_reasoning_steps:
        rubric.append("Provides a step-by-step explanation.")

    # 작은 변동을 넣어 매 round가 완전히 같지 않게 만든다.
    difficulty = max(1, recipe.difficulty + rng.choice([-1, 0, 0, 1]))
    specificity = max(1, recipe.specificity + rng.choice([0, 0, 1]))

    return CandidateExample(
        topic=source.topic,
        question=question,
        reference_answer=reference_answer,
        rubric=rubric,
        difficulty=difficulty,
        specificity=specificity,
    )


def solver_score(candidate: CandidateExample, solver_strength: int, rng: Random) -> float:
    """solver가 candidate를 얼마나 잘 풀지 점수로 시뮬레이션한다.

    solver_strength가 difficulty보다 높으면 잘 풀고, 낮으면 못 푼다.
    specificity가 높으면 강한 solver에는 도움이 되지만 약한 solver에는 부담이 된다.
    """

    base = 0.45 + 0.12 * (solver_strength - candidate.difficulty)
    specificity_bonus = 0.03 * candidate.specificity if solver_strength >= 4 else -0.03 * candidate.specificity
    noise = rng.uniform(-0.04, 0.04)
    score = base + specificity_bonus + noise
    return max(0.0, min(1.0, score))


def judge(candidate: CandidateExample, weak_score: float, strong_score: float) -> JudgeReport:
    """candidate가 학습 데이터로 적합한지 판단한다.

    CS task의 논문 설정을 단순화해 다음 조건을 사용한다.
    - strong solver는 충분히 잘 풀어야 한다.
    - weak solver는 아직 어려워해야 한다.
    - strong-weak gap이 충분히 커야 한다.
    """

    gap = strong_score - weak_score

    if strong_score < 0.65:
        return JudgeReport(
            accepted=False,
            weak_score=weak_score,
            strong_score=strong_score,
            gap=gap,
            reason="too_hard_for_strong_solver",
            suggestion="Lower difficulty or make the question more grounded and specific.",
        )

    if weak_score >= 0.50:
        return JudgeReport(
            accepted=False,
            weak_score=weak_score,
            strong_score=strong_score,
            gap=gap,
            reason="too_easy_for_weak_solver",
            suggestion="Increase difficulty and ask for a narrower edge case.",
        )

    if gap < 0.20:
        return JudgeReport(
            accepted=False,
            weak_score=weak_score,
            strong_score=strong_score,
            gap=gap,
            reason="insufficient_strong_weak_gap",
            suggestion="Make the task require a reasoning step that separates strong and weak solvers.",
        )

    return JudgeReport(
        accepted=True,
        weak_score=weak_score,
        strong_score=strong_score,
        gap=gap,
        reason="accepted",
        suggestion="Keep this style of question.",
    )


def update_recipe(recipe: Recipe, report: JudgeReport) -> Recipe:
    """judge feedback을 challenger recipe에 반영한다."""

    if report.reason == "too_easy_for_weak_solver":
        return Recipe(
            specificity=recipe.specificity + 1,
            difficulty=recipe.difficulty + 1,
            require_reasoning_steps=True,
        )

    if report.reason == "too_hard_for_strong_solver":
        return Recipe(
            specificity=recipe.specificity + 1,
            difficulty=max(1, recipe.difficulty - 1),
            require_reasoning_steps=recipe.require_reasoning_steps,
        )

    if report.reason == "insufficient_strong_weak_gap":
        return Recipe(
            specificity=recipe.specificity + 1,
            difficulty=recipe.difficulty,
            require_reasoning_steps=True,
        )

    return recipe


def run_loop_for_source(source: Source, rng: Random, max_rounds: int = 8) -> dict:
    """하나의 source에 대해 Autodata loop를 실행한다."""

    recipe = Recipe()
    trajectory = []

    for round_id in range(1, max_rounds + 1):
        candidate = challenger(source, recipe, rng)
        weak = solver_score(candidate, solver_strength=2, rng=rng)
        strong = solver_score(candidate, solver_strength=5, rng=rng)
        report = judge(candidate, weak, strong)

        trajectory.append(
            {
                "round": round_id,
                "recipe": asdict(recipe),
                "candidate": asdict(candidate),
                "judge_report": asdict(report),
            }
        )

        if report.accepted:
            return {
                "accepted": True,
                "rounds": round_id,
                "example": asdict(candidate),
                "judge_report": asdict(report),
                "trajectory": trajectory,
            }

        recipe = update_recipe(recipe, report)

    return {
        "accepted": False,
        "rounds": max_rounds,
        "example": None,
        "judge_report": trajectory[-1]["judge_report"],
        "trajectory": trajectory,
    }


def main() -> None:
    rng = Random(7)
    sources = load_sources()
    results = [run_loop_for_source(source, rng) for source in sources]

    with OUTPUT_PATH.open("w", encoding="utf-8") as f:
        for result in results:
            if result["accepted"]:
                f.write(json.dumps(result, ensure_ascii=False) + "\n")

    print("Autodata toy project")
    print(f"sources            : {len(sources)}")
    print(f"accepted examples  : {sum(1 for result in results if result['accepted'])}")
    print(f"output             : {OUTPUT_PATH}")
    print()

    for source, result in zip(sources, results):
        status = "ACCEPTED" if result["accepted"] else "FAILED"
        report = result["judge_report"]
        print(f"{status} | {source.topic} | rounds={result['rounds']}")
        print(
            f"  weak={report['weak_score']:.3f}, "
            f"strong={report['strong_score']:.3f}, "
            f"gap={report['gap']:.3f}, "
            f"reason={report['reason']}"
        )


if __name__ == "__main__":
    main()
