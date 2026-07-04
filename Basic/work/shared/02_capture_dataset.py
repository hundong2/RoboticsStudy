"""
Capture image samples for robotics perception or imitation-learning datasets.

Run:
    python shared/02_capture_dataset.py --camera opencv --source 0 --out data/camera_samples
    python shared/02_capture_dataset.py --camera picamera2 --out data/pi_samples

Keys:
    s: save current frame
    q: quit
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path


def timestamp() -> str:
    return time.strftime("%Y%m%d_%H%M%S") + f"_{int((time.time() % 1) * 1000):03d}"


def parse_source(value: str) -> int | str:
    return int(value) if value.isdigit() else value


def open_opencv_camera(source: int | str, width: int, height: int):
    import cv2

    cap = cv2.VideoCapture(source)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    if not cap.isOpened():
        raise RuntimeError(f"Could not open OpenCV camera: {source}")
    return cap


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--camera", choices=["auto", "opencv", "picamera2"], default="auto")
    parser.add_argument("--source", default="0")
    parser.add_argument("--out", default="data/camera_samples")
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--label", default="unlabeled")
    args = parser.parse_args()

    try:
        import cv2
    except ImportError as exc:
        raise SystemExit("Missing OpenCV. Install with: pip install opencv-python") from exc

    out_dir = Path(args.out)
    image_dir = out_dir / "images"
    image_dir.mkdir(parents=True, exist_ok=True)
    metadata_path = out_dir / "metadata.jsonl"

    camera_mode = args.camera
    picam2 = None
    cap = None
    if camera_mode == "picamera2":
        try:
            from picamera2 import Picamera2
        except ImportError as exc:
            raise SystemExit("Missing picamera2. On Raspberry Pi install with: sudo apt install python3-picamera2") from exc
        picam2 = Picamera2()
        config = picam2.create_preview_configuration(main={"size": (args.width, args.height), "format": "RGB888"})
        picam2.configure(config)
        picam2.start()
    else:
        cap = open_opencv_camera(parse_source(args.source), args.width, args.height)

    print("Press s to save, q to quit")
    try:
        while True:
            if picam2 is not None:
                frame = picam2.capture_array()
            else:
                ok, frame = cap.read()
                if not ok:
                    print("Frame read failed")
                    break

            cv2.imshow("dataset capture", frame)
            key = cv2.waitKey(1) & 0xFF
            if key == ord("q"):
                break
            if key == ord("s"):
                name = f"{timestamp()}.jpg"
                path = image_dir / name
                cv2.imwrite(str(path), frame)
                record = {
                    "image": str(path.as_posix()),
                    "label": args.label,
                    "time": time.time(),
                    "camera": camera_mode,
                }
                with metadata_path.open("a", encoding="utf-8") as f:
                    f.write(json.dumps(record, ensure_ascii=False) + "\n")
                print("saved", path)
    finally:
        if cap is not None:
            cap.release()
        if picam2 is not None:
            picam2.stop()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
