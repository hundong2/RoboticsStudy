"""
Parallel Box Decoding 교육용 데모.

이 파일은 NVIDIA LocateAnything 논문의 실제 구현이 아니다.
논문에서 중요한 아이디어인 "좌표를 토큰별로 순차 생성하지 말고,
박스 전체를 하나의 기하 단위로 병렬 예측한다"는 차이를 작은 코드로 보여준다.

실행:
    python Research\\2026-07-01_VLM_Native3D_Grounding\\parallel_box_decoding_demo.py
"""

from __future__ import annotations

from dataclasses import dataclass
from math import sqrt
from random import Random
from statistics import mean
from time import perf_counter, sleep


@dataclass(frozen=True)
class Box:
    """정규화된 bounding box.

    좌표는 0.0~1.0 범위라고 가정한다.
    x1, y1은 좌상단, x2, y2는 우하단이다.
    """

    x1: float
    y1: float
    x2: float
    y2: float

    def clamp_and_order(self) -> "Box":
        """좌표 범위와 순서를 보정한다.

        순차 토큰 생성에서는 x2가 x1보다 작게 나오는 등
        박스 기하가 깨질 수 있다. 실제 시스템에서는 후처리나
        구조적 디코더가 이런 문제를 줄여야 한다.
        """

        x1 = min(max(self.x1, 0.0), 1.0)
        y1 = min(max(self.y1, 0.0), 1.0)
        x2 = min(max(self.x2, 0.0), 1.0)
        y2 = min(max(self.y2, 0.0), 1.0)
        return Box(min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2))

    def center_error(self, target: "Box") -> float:
        """두 박스 중심점 사이의 거리.

        실제 grounding 평가는 IoU를 주로 쓰지만, 여기서는 코드가
        단순하도록 중심점 오차를 사용한다.
        """

        cx = (self.x1 + self.x2) / 2
        cy = (self.y1 + self.y2) / 2
        tcx = (target.x1 + target.x2) / 2
        tcy = (target.y1 + target.y2) / 2
        return sqrt((cx - tcx) ** 2 + (cy - tcy) ** 2)


def noisy(value: float, rng: Random, scale: float) -> float:
    """간단한 noise 모델.

    VLM이 좌표를 완벽히 맞추지 못하는 상황을 흉내 낸다.
    """

    return value + rng.uniform(-scale, scale)


def autoregressive_decode(target: Box, rng: Random) -> Box:
    """좌표를 x1 -> y1 -> x2 -> y2 순서로 생성하는 방식.

    각 좌표 토큰이 이전 토큰에 의존한다고 가정하므로 sleep을 네 번 둔다.
    이것은 실제 latency가 아니라 순차 의존성의 비용을 보여주기 위한 장치다.
    """

    tokens = []
    for value in (target.x1, target.y1, target.x2, target.y2):
        sleep(0.001)
        tokens.append(noisy(value, rng, scale=0.035))
    return Box(*tokens).clamp_and_order()


def parallel_box_decode(target: Box, rng: Random) -> Box:
    """박스 전체를 한 번에 예측하는 방식.

    논문식 Parallel Box Decoding의 핵심은 박스를 네 개 독립 토큰이 아니라
    하나의 구조화된 geometric unit으로 다루는 것이다.
    여기서는 sleep을 한 번만 두고, 더 작은 noise를 사용해 구조적 예측이
    박스 일관성에 유리하다는 직관을 표현한다.
    """

    sleep(0.001)
    return Box(
        noisy(target.x1, rng, scale=0.020),
        noisy(target.y1, rng, scale=0.020),
        noisy(target.x2, rng, scale=0.020),
        noisy(target.y2, rng, scale=0.020),
    ).clamp_and_order()


def benchmark(name: str, decoder, targets: list[Box], seed: int) -> None:
    """디코더의 실행 시간과 평균 중심점 오차를 출력한다."""

    rng = Random(seed)
    start = perf_counter()
    predictions = [decoder(target, rng) for target in targets]
    elapsed_ms = (perf_counter() - start) * 1000
    errors = [pred.center_error(target) for pred, target in zip(predictions, targets)]

    print(f"{name}")
    print(f"  boxes          : {len(targets)}")
    print(f"  elapsed        : {elapsed_ms:.2f} ms")
    print(f"  avg center err : {mean(errors):.4f}")
    print()


def main() -> None:
    """고정된 예제 박스들로 두 디코딩 방식을 비교한다."""

    targets = [
        Box(0.10, 0.12, 0.28, 0.34),
        Box(0.45, 0.18, 0.72, 0.40),
        Box(0.20, 0.55, 0.38, 0.82),
        Box(0.62, 0.58, 0.90, 0.88),
        Box(0.05, 0.70, 0.18, 0.94),
    ]

    benchmark("Autoregressive coordinate tokens", autoregressive_decode, targets, seed=7)
    benchmark("Parallel box decoding", parallel_box_decode, targets, seed=7)


if __name__ == "__main__":
    main()
