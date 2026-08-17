#!/usr/bin/env python3
"""Create a deterministic model manifest with SHA-256 and preprocessing metadata."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--model-id", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--labels", type=Path, required=True)
    parser.add_argument("--input-name", default="images")
    parser.add_argument("--input-shape", default="1,3,640,640")
    parser.add_argument("--color", choices=("RGB", "BGR"), default="RGB")
    parser.add_argument("--scale", type=float, default=1.0 / 255.0)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    dimensions = [int(value) for value in args.input_shape.split(",")]
    if len(dimensions) != 4 or any(value <= 0 for value in dimensions):
        parser.error("--input-shape must contain four positive dimensions, e.g. 1,3,640,640")
    labels = [line.strip() for line in args.labels.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not labels:
        parser.error("labels file is empty")

    manifest = {
        "schemaVersion": 1,
        "modelId": args.model_id,
        "version": args.version,
        "artifact": {"file": args.model.name, "format": "onnx", "sha256": sha256(args.model)},
        "input": {
            "name": args.input_name,
            "shape": dimensions,
            "layout": "NCHW",
            "color": args.color,
            "dataType": "float32",
            "scale": args.scale,
        },
        "labels": labels,
        "postprocessing": {"kind": "model-specific", "confidenceThreshold": 0.5, "nmsIouThreshold": 0.45},
        "runtime": {"preferred": "tensorrt-fp16", "engineBuildLocation": "target-device"},
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"manifest={args.output} sha256={manifest['artifact']['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
