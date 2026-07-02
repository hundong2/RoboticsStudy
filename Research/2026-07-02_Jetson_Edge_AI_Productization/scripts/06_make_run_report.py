import argparse
from pathlib import Path

import pandas as pd


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--title", default="Jetson Edge AI Run Report")
    parser.add_argument("--metrics", required=True)
    parser.add_argument("--output", default="reports/run_report.md")
    args = parser.parse_args()

    df = pd.read_csv(args.metrics)
    latency = df["latency_ms"].astype(float)
    fps = df["fps"].astype(float)

    report = f"""# {args.title}

## Summary

| Metric | Value |
| --- | ---: |
| Frames | {len(df)} |
| Latency mean ms | {latency.mean():.3f} |
| Latency median ms | {latency.median():.3f} |
| Latency p95 ms | {latency.quantile(0.95):.3f} |
| FPS mean | {fps.mean():.3f} |
| FPS median | {fps.median():.3f} |

## Notes

- Camera input:
- Model:
- Input size:
- Confidence threshold:
- Jetson power mode:
- TensorRT/ONNX/PyTorch runtime:

## False Positive Cases

| Image | Cause | Fix idea |
| --- | --- | --- |
| | | |

## False Negative Cases

| Image | Cause | Fix idea |
| --- | --- | --- |
| | | |

## Next Actions

- Add more data for failure cases.
- Compare PyTorch, ONNX Runtime, and TensorRT latency.
- Move preprocessing/postprocessing bottlenecks out of the hot path.
"""

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(report, encoding="utf-8")
    print(f"report written: {output.resolve()}")


if __name__ == "__main__":
    main()
