import argparse
import statistics
import time

import numpy as np
import onnxruntime as ort


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--onnx", required=True)
    parser.add_argument("--shape", nargs=4, type=int, default=[1, 3, 640, 640])
    parser.add_argument("--warmup", type=int, default=20)
    parser.add_argument("--repeat", type=int, default=100)
    args = parser.parse_args()

    providers = ["CUDAExecutionProvider", "CPUExecutionProvider"]
    available = ort.get_available_providers()
    selected = [p for p in providers if p in available]
    session = ort.InferenceSession(args.onnx, providers=selected)
    input_name = session.get_inputs()[0].name

    x = np.random.rand(*args.shape).astype(np.float32)

    for _ in range(args.warmup):
        session.run(None, {input_name: x})

    times = []
    for _ in range(args.repeat):
        start = time.perf_counter()
        session.run(None, {input_name: x})
        times.append((time.perf_counter() - start) * 1000.0)

    print(f"onnx: {args.onnx}")
    print(f"providers available: {available}")
    print(f"providers selected: {selected}")
    print(f"shape: {args.shape}")
    print(f"repeat: {args.repeat}")
    print(f"mean_ms: {statistics.mean(times):.3f}")
    print(f"median_ms: {statistics.median(times):.3f}")
    print(f"p95_ms: {np.percentile(times, 95):.3f}")
    print(f"min_ms: {min(times):.3f}")
    print(f"max_ms: {max(times):.3f}")


if __name__ == "__main__":
    main()
