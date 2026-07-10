"""
PAW toy project demo.

실행:
    python Research\\2026-07-10_Program_as_Weights_PAW\\paw_toy_project\\demo.py
"""

from __future__ import annotations

from pathlib import Path

from paw_toy import PawInterpreter, compile_program, load_program, save_program


ROOT = Path(__file__).resolve().parent
PROGRAM_DIR = ROOT / "programs"


def compile_examples() -> list[Path]:
    """세 가지 fuzzy function을 program artifact로 컴파일한다."""

    specs = [
        (
            "email_triage",
            "Classify if this email requires immediate attention. Return urgent or normal.",
        ),
        (
            "log_monitor",
            "Classify log lines. Return ALERT only when the line is worth interrupting the developer.",
        ),
        (
            "search_rerank",
            "Given a search query and candidate result, classify relevance for semantic reranking.",
        ),
    ]

    paths: list[Path] = []
    for slug, spec in specs:
        program = compile_program(spec, slug=slug)
        path = PROGRAM_DIR / f"{slug}.paw.json"
        save_program(program, path)
        paths.append(path)
    return paths


def run_examples(paths: list[Path]) -> None:
    """같은 interpreter로 여러 compiled program을 실행한다."""

    interpreter = PawInterpreter()
    test_inputs = {
        "email_triage": [
            "Need your signature by EOD before the client is blocked.",
            "Sharing the notes for next month, no action needed.",
        ],
        "log_monitor": [
            "Traceback most recent call last: RuntimeError",
            "[step 2100] loss=0.031 lr=0.0001",
        ],
        "search_rerank": [
            "query=opencv build from source candidate=official OpenCV build guide",
            "query=svm margin candidate=banana bread recipe",
        ],
    }

    for path in paths:
        program = load_program(path)
        print(f"\n=== {program.slug} ===")
        print(f"pseudo task: {program.pseudo_program.task}")
        print(f"program file: {path.name}")

        for text in test_inputs[program.slug]:
            output = interpreter.run(program, text)
            scores = interpreter.debug_scores(program, text)
            print(f"input : {text}")
            print(f"output: {output}")
            print(f"scores: {scores}")


def main() -> None:
    paths = compile_examples()
    print("Compiled PAW toy programs:")
    for path in paths:
        print(f"- {path}")

    run_examples(paths)


if __name__ == "__main__":
    main()
