"""
Raspberry Pi Camera Module preview with Picamera2.

Run:
    python raspberry_pi/01_picamera2_preview.py

Press q to quit.
"""

from __future__ import annotations

import argparse


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    args = parser.parse_args()

    try:
        import cv2
        from picamera2 import Picamera2
    except ImportError as exc:
        raise SystemExit("Install with: sudo apt install python3-picamera2 python3-opencv") from exc

    picam2 = Picamera2()
    config = picam2.create_preview_configuration(main={"size": (args.width, args.height), "format": "RGB888"})
    picam2.configure(config)
    picam2.start()

    try:
        while True:
            frame = picam2.capture_array()
            cv2.imshow("Raspberry Pi Camera Module", frame)
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break
    finally:
        picam2.stop()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
