"""
Jetson CSI camera preview through a GStreamer pipeline.

Run:
    python jetson_orin/01_jetson_camera_preview.py

If you use a USB camera instead, use:
    python shared/01_opencv_camera_preview.py --source 0
"""

from __future__ import annotations

import argparse


def gstreamer_pipeline(
    sensor_id: int = 0,
    capture_width: int = 1280,
    capture_height: int = 720,
    display_width: int = 960,
    display_height: int = 540,
    framerate: int = 30,
    flip_method: int = 0,
) -> str:
    return (
        f"nvarguscamerasrc sensor-id={sensor_id} ! "
        f"video/x-raw(memory:NVMM), width=(int){capture_width}, height=(int){capture_height}, "
        f"format=(string)NV12, framerate=(fraction){framerate}/1 ! "
        f"nvvidconv flip-method={flip_method} ! "
        f"video/x-raw, width=(int){display_width}, height=(int){display_height}, format=(string)BGRx ! "
        "videoconvert ! video/x-raw, format=(string)BGR ! appsink"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sensor-id", type=int, default=0)
    parser.add_argument("--width", type=int, default=960)
    parser.add_argument("--height", type=int, default=540)
    parser.add_argument("--flip", type=int, default=0)
    args = parser.parse_args()

    try:
        import cv2
    except ImportError as exc:
        raise SystemExit("Missing OpenCV. Install an OpenCV build with GStreamer support.") from exc

    pipeline = gstreamer_pipeline(
        sensor_id=args.sensor_id,
        display_width=args.width,
        display_height=args.height,
        flip_method=args.flip,
    )
    print("GStreamer pipeline:")
    print(pipeline)
    cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)
    if not cap.isOpened():
        raise SystemExit("Could not open Jetson CSI camera. Check ribbon cable, sensor-id, and nvargus-daemon.")

    while True:
        ok, frame = cap.read()
        if not ok:
            break
        cv2.imshow("Jetson CSI camera", frame)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
