#!/usr/bin/env python3
"""Run an ONNX model and report output shapes, numeric ranges, and average latency."""

from __future__ import annotations

import argparse
import time
from pathlib import Path

import numpy as np
import onnxruntime as ort


def parse_shapes(items: list[str]) -> dict[str, tuple[int, ...]]:
    result: dict[str, tuple[int, ...]] = {}
    for item in items:
        try:
            name, dimensions = item.split("=", 1)
            shape = tuple(int(value) for value in dimensions.split(","))
        except ValueError as error:
            raise ValueError(f"Invalid --shape '{item}'; expected name=1,3,640,640") from error
        if not shape or any(value <= 0 for value in shape):
            raise ValueError(f"Shape values must be positive: {item}")
        result[name] = shape
    return result


def providers_for(name: str) -> list[str | tuple[str, dict[str, object]]]:
    if name == "cpu":
        return ["CPUExecutionProvider"]
    if name == "cuda":
        return ["CUDAExecutionProvider", "CPUExecutionProvider"]
    return [
        ("TensorrtExecutionProvider", {"trt_fp16_enable": True, "trt_timing_cache_enable": True}),
        "CUDAExecutionProvider",
        "CPUExecutionProvider",
    ]


def concrete_shape(name: str, model_shape: list[int | str | None], overrides: dict[str, tuple[int, ...]]) -> tuple[int, ...]:
    if name in overrides:
        return overrides[name]
    if any(not isinstance(value, int) or value <= 0 for value in model_shape):
        raise ValueError(f"Input '{name}' is dynamic; provide --shape {name}=1,3,640,640")
    return tuple(int(value) for value in model_shape)


def numpy_type(ort_type: str) -> np.dtype:
    supported = {
        "tensor(float)": np.dtype(np.float32),
        "tensor(float16)": np.dtype(np.float16),
        "tensor(uint8)": np.dtype(np.uint8),
        "tensor(int64)": np.dtype(np.int64),
    }
    if ort_type not in supported:
        raise ValueError(f"Unsupported demo input type: {ort_type}")
    return supported[ort_type]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("--provider", choices=("cpu", "cuda", "tensorrt"), default="cpu")
    parser.add_argument("--shape", action="append", default=[], help="Input override, e.g. images=1,3,640,640")
    parser.add_argument("--input-npy", type=Path, help="Use a .npy array for a single-input model")
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be at least 1")

    required_provider = {
        "cpu": "CPUExecutionProvider",
        "cuda": "CUDAExecutionProvider",
        "tensorrt": "TensorrtExecutionProvider",
    }[args.provider]
    available = ort.get_available_providers()
    if required_provider not in available:
        raise RuntimeError(
            f"Requested provider '{required_provider}' is not installed. "
            f"Available providers: {', '.join(available)}"
        )
    requested = providers_for(args.provider)
    session = ort.InferenceSession(str(args.model), providers=requested)
    print("available_providers=" + ",".join(available))
    print("active_providers=" + ",".join(session.get_providers()))

    overrides = parse_shapes(args.shape)
    rng = np.random.default_rng(args.seed)
    feeds: dict[str, np.ndarray] = {}
    for index, item in enumerate(session.get_inputs()):
        if args.input_npy and len(session.get_inputs()) == 1:
            value = np.load(args.input_npy)
        else:
            shape = concrete_shape(item.name, item.shape, overrides)
            dtype = numpy_type(item.type)
            value = rng.random(shape).astype(dtype) if np.issubdtype(dtype, np.floating) else np.zeros(shape, dtype=dtype)
        feeds[item.name] = value
        print(f"input[{index}] name={item.name} shape={value.shape} dtype={value.dtype}")

    session.run(None, feeds)  # warm-up
    started = time.perf_counter()
    outputs: list[np.ndarray] = []
    for _ in range(args.runs):
        outputs = session.run(None, feeds)
    elapsed_ms = (time.perf_counter() - started) * 1000.0 / args.runs

    for index, value in enumerate(outputs):
        numeric = np.asarray(value)
        finite = numeric[np.isfinite(numeric)] if np.issubdtype(numeric.dtype, np.number) else np.array([])
        stats = "non-numeric-or-empty"
        if finite.size:
            stats = f"min={finite.min():.6g} max={finite.max():.6g} mean={finite.mean():.6g}"
        print(f"output[{index}] shape={numeric.shape} dtype={numeric.dtype} {stats}")
    print(f"average_inference_ms={elapsed_ms:.3f} runs={args.runs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
