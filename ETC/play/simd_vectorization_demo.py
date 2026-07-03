"""Scalar loop vs vectorized NumPy demo.

NumPy normally calls optimized native code that can use SIMD paths such as
SSE, AVX, AVX2, or NEON depending on the installed wheel and CPU.

Run:
    uv run python play/simd_vectorization_demo.py
"""

from __future__ import annotations

from time import perf_counter

import numpy as np


def scalar_dot(a: np.ndarray, b: np.ndarray) -> float:
    total = 0.0
    for i in range(a.size):
        total += float(a[i]) * float(b[i])
    return total


def main() -> None:
    rng = np.random.default_rng(11)
    size = 1_000_000
    a = rng.random(size, dtype=np.float32)
    b = rng.random(size, dtype=np.float32)

    t0 = perf_counter()
    scalar = scalar_dot(a, b)
    t1 = perf_counter()

    t2 = perf_counter()
    vectorized = float(np.dot(a, b))
    t3 = perf_counter()

    print(f"scalar result    : {scalar:.3f}")
    print(f"vectorized result: {vectorized:.3f}")
    print(f"scalar time      : {t1 - t0:.4f}s")
    print(f"vectorized time  : {t3 - t2:.4f}s")
    print(f"speedup          : {(t1 - t0) / max(t3 - t2, 1e-9):.1f}x")
    print()
    print("NumPy build configuration:")
    np.show_config()


if __name__ == "__main__":
    main()
