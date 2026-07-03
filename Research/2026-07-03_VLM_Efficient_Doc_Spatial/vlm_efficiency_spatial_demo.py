"""
2026-07-03 VLM 리서치 노트 실습 코드.

실제 VLM을 내려받아 실행하지 않고도 다음 아이디어를 숫자로 확인한다.

1. Unlimited OCR / R-SWA:
   긴 출력에서 full attention KV cache와 fixed sliding window cache가
   어떻게 달라지는지 비교한다.

2. SmolDocling / DocTags:
   단순 OCR 텍스트와 구조화 markup의 차이를 작은 문서 예제로 보여준다.

3. World2VLM:
   세계 모델을 추론 때마다 호출하는 방식과 학습 시점에 distillation해서
   VLM 내부에 넣는 방식의 비용 차이를 단순 모델로 비교한다.

실행:
    python Research\\2026-07-03_VLM_Efficient_Doc_Spatial\\vlm_efficiency_spatial_demo.py
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class AttentionConfig:
    """KV cache 비용을 간단히 추정하기 위한 설정."""

    reference_tokens: int = 1024
    window_size: int = 128
    layers: int = 24
    hidden_size: int = 2048
    bytes_per_value: int = 2


def kv_cache_mb(tokens: int, config: AttentionConfig) -> float:
    """KV cache 메모리를 MB 단위로 추정한다.

    key와 value 두 텐서를 저장한다고 가정한다.
    실제 모델의 head layout과 dtype에 따라 값은 달라지지만,
    출력 길이에 따른 증가 형태를 이해하기에는 충분하다.
    """

    kv_tensors = 2
    total_bytes = (
        tokens
        * config.layers
        * config.hidden_size
        * kv_tensors
        * config.bytes_per_value
    )
    return total_bytes / (1024 * 1024)


def demo_rswa_cache() -> None:
    """Full attention과 R-SWA의 cache 증가를 비교한다."""

    config = AttentionConfig()
    generated_lengths = [256, 1024, 4096, 8192, 16384, 32768]

    print("1) R-SWA cache cost demo")
    print(f"reference_tokens={config.reference_tokens}, window_size={config.window_size}")
    print(f"{'generated':>10} | {'full MB':>10} | {'r-swa MB':>10} | {'saving':>8}")
    print("-" * 52)

    for generated in generated_lengths:
        full_tokens = config.reference_tokens + generated
        rswa_tokens = config.reference_tokens + min(generated, config.window_size)
        full_mb = kv_cache_mb(full_tokens, config)
        rswa_mb = kv_cache_mb(rswa_tokens, config)
        saving = 1.0 - (rswa_mb / full_mb)
        print(f"{generated:>10} | {full_mb:>10.1f} | {rswa_mb:>10.1f} | {saving:>7.1%}")

    print("why: 긴 문서 OCR에서는 출력 길이가 길어질수록 cache가 병목이 된다.")
    print("     R-SWA는 reference는 유지하고 오래된 출력만 잊어 메모리 상한을 만든다.")
    print()


@dataclass(frozen=True)
class DocumentElement:
    """문서 안의 구조적 요소."""

    kind: str
    text: str
    bbox: tuple[int, int, int, int]


def to_plain_ocr(elements: list[DocumentElement]) -> str:
    """일반 OCR처럼 텍스트만 이어 붙인다."""

    return "\n".join(element.text for element in elements)


def to_doctags(elements: list[DocumentElement]) -> str:
    """SmolDocling의 DocTags 아이디어를 단순화한 구조화 출력.

    핵심은 텍스트뿐 아니라 요소 종류와 위치를 같이 남기는 것이다.
    이 정보가 있어야 나중에 RAG, 표 파싱, UI 재구성에서 손실이 줄어든다.
    """

    lines = []
    for element in elements:
        x1, y1, x2, y2 = element.bbox
        lines.append(
            f"<{element.kind} bbox='{x1},{y1},{x2},{y2}'>"
            f"{element.text}"
            f"</{element.kind}>"
        )
    return "\n".join(lines)


def demo_doctags() -> None:
    """단순 OCR과 구조화 문서 변환의 차이를 보여준다."""

    elements = [
        DocumentElement("title", "Quarterly Robot Inspection", (10, 10, 420, 42)),
        DocumentElement("paragraph", "Arm torque exceeded threshold twice.", (10, 60, 520, 95)),
        DocumentElement("table", "joint,max_nm,observed_nm\nJ1,80,86\nJ2,65,59", (10, 120, 500, 230)),
        DocumentElement("equation", "margin = 1 / ||w||", (10, 250, 320, 285)),
    ]

    print("2) SmolDocling-style structured document demo")
    print("[plain OCR]")
    print(to_plain_ocr(elements))
    print()
    print("[DocTags-like output]")
    print(to_doctags(elements))
    print()
    print("why: 단순 OCR은 위치와 요소 타입을 잃는다.")
    print("     DocTags처럼 구조를 남기면 표, 수식, 문단을 후처리하기 쉽다.")
    print()


def estimate_world_model_cost(
    queries_per_day: int,
    world_model_cost_per_query: float,
    vlm_cost_per_query: float,
    distillation_training_cost: float,
) -> tuple[float, float]:
    """추론 시 world model 결합 방식과 distillation 방식의 일일 비용 비교."""

    test_time_coupled = queries_per_day * (world_model_cost_per_query + vlm_cost_per_query)
    distilled = distillation_training_cost + queries_per_day * vlm_cost_per_query
    return test_time_coupled, distilled


def demo_world2vlm_cost() -> None:
    """World2VLM식 training-time distillation이 왜 필요한지 비용으로 보여준다."""

    print("3) World2VLM distillation cost demo")
    print(f"{'queries/day':>12} | {'test-time world model':>22} | {'distilled VLM':>14}")
    print("-" * 58)

    for queries in [100, 1_000, 10_000, 100_000]:
        coupled, distilled = estimate_world_model_cost(
            queries_per_day=queries,
            world_model_cost_per_query=0.02,
            vlm_cost_per_query=0.003,
            distillation_training_cost=50.0,
        )
        print(f"{queries:>12} | ${coupled:>21.2f} | ${distilled:>13.2f}")

    print("why: world model을 매 추론마다 호출하면 사용량이 늘수록 비용이 선형 증가한다.")
    print("     학습 때 imagination을 증류하면 초기 학습 비용은 들지만 대량 추론이 싸진다.")
    print()


def main() -> None:
    demo_rswa_cache()
    demo_doctags()
    demo_world2vlm_cost()


if __name__ == "__main__":
    main()
