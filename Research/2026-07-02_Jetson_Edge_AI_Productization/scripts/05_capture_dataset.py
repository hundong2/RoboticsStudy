import argparse
import time
from pathlib import Path

import cv2

from importlib.machinery import SourceFileLoader


camera = SourceFileLoader("camera_preview", str(Path(__file__).with_name("01_camera_preview.py"))).load_module()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", choices=["csi", "usb", "rtsp", "file"], default="csi")
    parser.add_argument("--device", type=int, default=0)
    parser.add_argument("--sensor-id", type=int, default=0)
    parser.add_argument("--uri", default="")
    parser.add_argument("--file", default="")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--output", default="data/raw/camera")
    parser.add_argument("--interval", type=int, default=10, help="save one frame every N frames")
    args = parser.parse_args()

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    cap = camera.open_capture(args)
    if not cap.isOpened():
        raise RuntimeError("camera open failed")

    frame_idx = 0
    saved = 0

    while True:
        ok, frame = cap.read()
        if not ok:
            print("frame read failed")
            break

        preview = frame.copy()
        cv2.putText(
            preview,
            f"saved: {saved} | q: quit | s: save now",
            (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            (0, 255, 0),
            2,
        )
        cv2.imshow("capture dataset", preview)

        key = cv2.waitKey(1) & 0xFF
        should_save = frame_idx % args.interval == 0 or key == ord("s")
        if should_save:
            path = output_dir / f"frame_{int(time.time())}_{frame_idx:06d}.jpg"
            cv2.imwrite(str(path), frame)
            saved += 1

        frame_idx += 1
        if key in (27, ord("q")):
            break

    cap.release()
    cv2.destroyAllWindows()
    print(f"saved images: {saved}")
    print(f"output: {output_dir.resolve()}")


if __name__ == "__main__":
    main()
