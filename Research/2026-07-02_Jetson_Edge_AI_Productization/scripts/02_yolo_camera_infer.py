import argparse
import csv
import time
from pathlib import Path

import cv2
from ultralytics import YOLO

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
    parser.add_argument("--model", default="yolo11n.pt")
    parser.add_argument("--conf", type=float, default=0.25)
    parser.add_argument("--imgsz", type=int, default=640)
    parser.add_argument("--save-video", default="")
    parser.add_argument("--save-metrics", default="")
    args = parser.parse_args()

    model = YOLO(args.model)
    cap = camera.open_capture(args)
    if not cap.isOpened():
        raise RuntimeError("camera open failed")

    writer = None
    if args.save_video:
        Path(args.save_video).parent.mkdir(parents=True, exist_ok=True)
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        writer = cv2.VideoWriter(args.save_video, fourcc, args.fps, (args.width, args.height))

    metric_file = None
    metric_writer = None
    if args.save_metrics:
        Path(args.save_metrics).parent.mkdir(parents=True, exist_ok=True)
        metric_file = open(args.save_metrics, "w", newline="", encoding="utf-8")
        metric_writer = csv.writer(metric_file)
        metric_writer.writerow(["frame", "latency_ms", "fps", "detections"])

    frame_idx = 0
    fps_value = 0.0

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                print("frame read failed")
                break

            start = time.perf_counter()
            results = model.predict(frame, imgsz=args.imgsz, conf=args.conf, verbose=False)
            latency_ms = (time.perf_counter() - start) * 1000.0
            fps_value = 0.9 * fps_value + 0.1 * (1000.0 / latency_ms)

            annotated = results[0].plot()
            detections = len(results[0].boxes) if results and results[0].boxes is not None else 0
            cv2.putText(
                annotated,
                f"{latency_ms:.1f} ms | {fps_value:.1f} FPS | det {detections}",
                (20, 40),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.9,
                (0, 255, 0),
                2,
            )

            if writer is not None:
                writer.write(annotated)
            if metric_writer is not None:
                metric_writer.writerow([frame_idx, f"{latency_ms:.3f}", f"{fps_value:.3f}", detections])

            cv2.imshow("yolo camera inference", annotated)
            frame_idx += 1
            if cv2.waitKey(1) & 0xFF in (27, ord("q")):
                break
    finally:
        cap.release()
        if writer is not None:
            writer.release()
        if metric_file is not None:
            metric_file.close()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
