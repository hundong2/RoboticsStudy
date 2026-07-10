"""
Program-as-Weights toy implementation.

논문 "Program-as-Weights"의 실제 neural compiler/LoRA/interpreter 구현이 아니다.
대신 핵심 소프트웨어 구조를 작게 재현한다.

대응 관계:
    natural-language spec  -> spec 문자열
    pseudo compiler        -> spec 정규화 + 예시 생성
    continuous PEFT        -> keyword weight dictionary
    frozen interpreter     -> 공통 scoring runtime
    PAW program artifact   -> .paw.json 파일

핵심:
    compile은 한 번 수행하고, run은 로컬에서 반복 수행한다.
"""

from __future__ import annotations

import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class PseudoProgram:
    """사람이 읽을 수 있는 discrete program component."""

    task: str
    labels: list[str]
    examples: list[dict[str, str]]
    notes: str


@dataclass(frozen=True)
class PawProgram:
    """컴파일된 PAW toy program.

    pseudo_program은 논문의 discrete half에 해당한다.
    weights는 논문의 continuous PEFT component를 단순화한 것이다.
    """

    slug: str
    spec: str
    pseudo_program: PseudoProgram
    weights: dict[str, dict[str, float]]
    default_label: str
    version: int = 1


def normalize_text(text: str) -> list[str]:
    """간단한 tokenizer.

    실제 interpreter는 neural tokenizer와 transformer를 사용한다.
    여기서는 로컬 실행 가능성을 보여주기 위해 정규식 기반 토큰화를 쓴다.
    """

    return re.findall(r"[a-z0-9_]+", text.lower())


def pseudo_compile(spec: str) -> PseudoProgram:
    """자연어 spec을 pseudo-program으로 정리한다.

    실제 논문에서는 off-the-shelf 4B 모델이 task restatement와 입출력 예시를 만든다.
    여기서는 spec 안의 키워드로 몇 가지 demo task를 구분한다.
    """

    lowered = spec.lower()

    if "email" in lowered or "urgent" in lowered:
        return PseudoProgram(
            task="Classify whether a message requires immediate attention.",
            labels=["urgent", "normal"],
            examples=[
                {"input": "Need your signature by EOD.", "output": "urgent"},
                {"input": "Sharing the slides for next month.", "output": "normal"},
            ],
            notes="Return only one label. Prefer urgent when there is a deadline, blocker, outage, or approval needed today.",
        )

    if "log" in lowered or "alert" in lowered:
        return PseudoProgram(
            task="Classify log lines as alert-worthy or quiet.",
            labels=["ALERT", "QUIET"],
            examples=[
                {"input": "Traceback most recent call last", "output": "ALERT"},
                {"input": "[step 100] loss=0.05 lr=0.0001", "output": "QUIET"},
            ],
            notes="Alert on errors, crashes, checkpoint completion, training completion, or blocked states.",
        )

    if "search" in lowered or "rerank" in lowered:
        return PseudoProgram(
            task="Rate whether a candidate search result satisfies the query intent.",
            labels=["exact_match", "highly_relevant", "somewhat_relevant", "not_relevant"],
            examples=[
                {"input": "query=opencv install candidate=OpenCV build guide", "output": "highly_relevant"},
                {"input": "query=svm margin candidate=recipe blog", "output": "not_relevant"},
            ],
            notes="Respect constraints and exclusions. Prefer exact_match only when the candidate directly answers the query.",
        )

    return PseudoProgram(
        task="Classify text according to the user specification.",
        labels=["yes", "no"],
        examples=[],
        notes="Return only one label.",
    )


def compile_weights(pseudo: PseudoProgram) -> dict[str, dict[str, float]]:
    """pseudo-program에서 toy continuous weights를 만든다.

    실제 PAW에서는 compiler hidden state가 LoRA mapper를 거쳐 layer별 adapter가 된다.
    여기서는 label별 keyword score table을 만든다.
    """

    label_weights: dict[str, dict[str, float]] = {label: {} for label in pseudo.labels}

    seed_keywords = {
        "urgent": ["eod", "today", "blocked", "asap", "signature", "approval", "deadline", "outage"],
        "normal": ["sharing", "next", "later", "reference", "newsletter", "optional"],
        "ALERT": ["traceback", "error", "failed", "exception", "complete", "checkpoint", "blocked", "crash"],
        "QUIET": ["loss", "lr", "step", "epoch", "progress", "debug"],
        "exact_match": ["exact", "official", "guide", "reference", "documentation"],
        "highly_relevant": ["install", "build", "tutorial", "paper", "implementation"],
        "somewhat_relevant": ["overview", "intro", "related", "summary"],
        "not_relevant": ["recipe", "travel", "movie", "unrelated", "blog"],
        "yes": ["yes", "true", "match"],
        "no": ["no", "false", "none"],
    }

    for label in pseudo.labels:
        for keyword in seed_keywords.get(label, []):
            label_weights[label][keyword] = 1.0

    # 예시 input에 등장한 토큰도 해당 output label에 약한 weight로 추가한다.
    for example in pseudo.examples:
        output = example["output"]
        if output not in label_weights:
            continue
        for token in normalize_text(example["input"]):
            label_weights[output][token] = label_weights[output].get(token, 0.0) + 0.35

    return label_weights


def compile_program(spec: str, slug: str) -> PawProgram:
    """spec을 toy PAW program으로 컴파일한다."""

    pseudo = pseudo_compile(spec)
    weights = compile_weights(pseudo)
    return PawProgram(
        slug=slug,
        spec=spec,
        pseudo_program=pseudo,
        weights=weights,
        default_label=pseudo.labels[-1],
    )


def save_program(program: PawProgram, path: Path) -> None:
    """compiled program artifact를 JSON 파일로 저장한다."""

    path.parent.mkdir(parents=True, exist_ok=True)
    payload = asdict(program)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")


def load_program(path: Path) -> PawProgram:
    """JSON program artifact를 로드한다."""

    payload = json.loads(path.read_text(encoding="utf-8"))
    pseudo = PseudoProgram(**payload["pseudo_program"])
    return PawProgram(
        slug=payload["slug"],
        spec=payload["spec"],
        pseudo_program=pseudo,
        weights=payload["weights"],
        default_label=payload["default_label"],
        version=payload["version"],
    )


class PawInterpreter:
    """고정된 local interpreter.

    여러 PawProgram을 hot-load해 서로 다른 fuzzy function처럼 실행한다.
    이 구조가 논문의 "one runtime, many programs" 아이디어와 대응된다.
    """

    def run(self, program: PawProgram, text: str) -> str:
        tokens = normalize_text(text)
        scores = {label: 0.0 for label in program.pseudo_program.labels}

        for label, weights in program.weights.items():
            for token in tokens:
                scores[label] += weights.get(token, 0.0)

        # 모든 score가 0이면 default label을 반환한다.
        best_label, best_score = max(scores.items(), key=lambda item: item[1])
        if best_score <= 0:
            return program.default_label
        return best_label

    def debug_scores(self, program: PawProgram, text: str) -> dict[str, float]:
        """학습용: label별 score를 반환한다."""

        tokens = normalize_text(text)
        scores = {label: 0.0 for label in program.pseudo_program.labels}
        for label, weights in program.weights.items():
            for token in tokens:
                scores[label] += weights.get(token, 0.0)
        return scores
