"""
YOLO camera inference starter.

Run:
    python shared/03_yolo_camera_infer.py --source 0

Install:
    pip install ultralytics opencv-python

On Jetson, prefer a JetPack-compatible PyTorch/Ultralytics setup. If that is
heavy, export to ONNX/TensorRT later after the basic camera path works.
"""

from __future__ import annotations

import argparse


def parse_source(value: str) -> int | str:
    return int(value) if value.isdigit() else value


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default="0")
    parser.add_argument("--model", default="yolov8n.pt")
    parser.add_argument("--conf", type=float, default=0.35)
    args = parser.parse_args()

    try:
        import cv2
        from ultralytics import YOLO
    except ImportError as exc:
        raise SystemExit("Install dependencies: pip install ultralytics opencv-python") from exc

    model = YOLO(args.model)
    cap = cv2.VideoCapture(parse_source(args.source))
    if not cap.isOpened():
        raise SystemExit(f"Could not open camera source: {args.source}")

    while True:
        ok, frame = cap.read()
        if not ok:
            break
        results = model.predict(frame, conf=args.conf, verbose=False)
        annotated = results[0].plot()
        cv2.imshow("YOLO camera inference", annotated)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
