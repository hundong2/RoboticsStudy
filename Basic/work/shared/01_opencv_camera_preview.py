"""
OpenCV camera preview for USB cameras and generic V4L2 devices.

Run:
    python shared/01_opencv_camera_preview.py --source 0
    python shared/01_opencv_camera_preview.py --source /dev/video0

Press q to quit.
"""

from __future__ import annotations

import argparse
import time


def parse_source(value: str) -> int | str:
    return int(value) if value.isdigit() else value


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default="0", help="Camera index or video path. Default: 0")
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    args = parser.parse_args()

    try:
        import cv2
    except ImportError as exc:
        raise SystemExit("Missing OpenCV. Install with: pip install opencv-python") from exc

    cap = cv2.VideoCapture(parse_source(args.source))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)

    if not cap.isOpened():
        raise SystemExit(f"Could not open camera source: {args.source}")

    last = time.time()
    frames = 0
    fps = 0.0
    while True:
        ok, frame = cap.read()
        if not ok:
            print("Frame read failed")
            break
        frames += 1
        now = time.time()
        if now - last >= 1.0:
            fps = frames / (now - last)
            frames = 0
            last = now
        cv2.putText(frame, f"FPS: {fps:.1f}", (12, 32), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        cv2.imshow("OpenCV camera preview", frame)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
