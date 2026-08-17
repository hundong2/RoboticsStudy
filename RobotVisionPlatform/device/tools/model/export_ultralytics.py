#!/usr/bin/env python3
"""Export an Ultralytics checkpoint to ONNX and copy it to a stable bundle path."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from ultralytics import YOLO


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--weights", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--image-size", type=int, default=640)
    parser.add_argument("--opset", type=int, default=17)
    parser.add_argument("--dynamic", action="store_true")
    parser.add_argument("--simplify", action="store_true")
    args = parser.parse_args()

    model = YOLO(str(args.weights))
    exported = Path(model.export(
        format="onnx",
        imgsz=args.image_size,
        opset=args.opset,
        dynamic=args.dynamic,
        simplify=args.simplify,
    ))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if exported.resolve() != args.output.resolve():
        shutil.copy2(exported, args.output)
    print(f"onnx={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

