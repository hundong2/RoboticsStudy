"""
R-SWA(Reference Sliding Window Attention) 메모리 구조 학습용 데모.

이 코드는 Unlimited OCR의 실제 attention kernel이 아니다.
대신 Full Attention과 R-SWA가 출력 길이에 따라 얼마나 다른 cache 크기를
가지는지 숫자로 비교한다.

핵심 개념:
    Full Attention cache = reference tokens + all generated tokens
    R-SWA cache          = reference tokens + recent generated tokens only

실행:
    python Research\\2026-07-02_UnlimitedOCR_R-SWA_VLM\\rswa_memory_demo.py
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class CacheConfig:
    """attention cache를 단순 토큰 개수로 계산하기 위한 설정."""

    reference_tokens: int = 512
    window_size: int = 128


def full_attention_cache_tokens(config: CacheConfig, generated_tokens: int) -> int:
    """표준 full attention의 cache 토큰 수.

    생성한 모든 output token의 key/value를 계속 보관한다고 가정한다.
    따라서 generated_tokens가 길어질수록 cache도 선형으로 증가한다.
    """

    return config.reference_tokens + generated_tokens


def rswa_cache_tokens(config: CacheConfig, generated_tokens: int) -> int:
    """R-SWA의 cache 토큰 수.

    reference token은 항상 접근 가능하게 유지하고,
    generated token은 최근 window_size개만 유지한다고 가정한다.
    그래서 출력이 아무리 길어져도 generated 쪽 cache는 window_size를 넘지 않는다.
    """

    working_memory = min(generated_tokens, config.window_size)
    return config.reference_tokens + working_memory


def estimate_cache_mb(cache_tokens: int, layers: int = 24, hidden_size: int = 2048) -> float:
    """대략적인 KV cache 메모리 사용량을 MB 단위로 추정한다.

    실제 모델마다 head 수, dtype, tensor layout이 다르므로 정확한 값은 아니다.
    여기서는 fp16/bf16처럼 값 하나가 2 bytes라고 보고,
    key와 value 두 tensor를 저장한다고 가정한다.
    """

    bytes_per_value = 2
    kv_tensors = 2
    total_bytes = cache_tokens * layers * hidden_size * kv_tensors * bytes_per_value
    return total_bytes / (1024 * 1024)


def main() -> None:
    """여러 출력 길이에 대해 Full Attention과 R-SWA를 비교한다."""

    config = CacheConfig(reference_tokens=512, window_size=128)
    generated_lengths = [128, 512, 1024, 4096, 8192, 16384, 32768]

    print("R-SWA memory demo")
    print(f"reference_tokens={config.reference_tokens}, window_size={config.window_size}")
    print()
    print(f"{'generated':>10} | {'full tokens':>11} | {'r-swa tokens':>11} | {'full MB':>9} | {'r-swa MB':>9}")
    print("-" * 70)

    for generated in generated_lengths:
        full_tokens = full_attention_cache_tokens(config, generated)
        rswa_tokens = rswa_cache_tokens(config, generated)
        print(
            f"{generated:>10} | "
            f"{full_tokens:>11} | "
            f"{rswa_tokens:>11} | "
            f"{estimate_cache_mb(full_tokens):>9.1f} | "
            f"{estimate_cache_mb(rswa_tokens):>9.1f}"
        )

    print()
    print("해석: Full attention은 출력 길이에 따라 cache가 계속 커지고,")
    print("R-SWA는 reference + window 크기 근처에서 cache가 고정된다.")


if __name__ == "__main__":
    main()
