import argparse
import time
from pathlib import Path

import cv2
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision


def draw_detection_result(image_bgr, detection_result):
    for detection in detection_result.detections:
        bbox = detection.bounding_box
        x1 = int(bbox.origin_x)
        y1 = int(bbox.origin_y)
        x2 = int(bbox.origin_x + bbox.width)
        y2 = int(bbox.origin_y + bbox.height)

        category = detection.categories[0]
        name = category.category_name or str(category.index)
        label = f"{name} {category.score:.2f}"

        cv2.rectangle(image_bgr, (x1, y1), (x2, y2), (40, 180, 40), 2)
        cv2.putText(
            image_bgr,
            label,
            (x1, max(20, y1 - 8)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.55,
            (40, 180, 40),
            2,
            cv2.LINE_AA,
        )


def parse_args():
    parser = argparse.ArgumentParser(description="MediaPipe object detection from a webcam.")
    parser.add_argument("--model", default="assets/models/efficientdet_lite0_int8.tflite", help="TFLite model path.")
    parser.add_argument("--camera", type=int, default=0, help="OpenCV camera index.")
    parser.add_argument("--score", type=float, default=0.35, help="Score threshold.")
    parser.add_argument("--max-results", type=int, default=10, help="Maximum number of detections.")
    return parser.parse_args()


def main():
    args = parse_args()
    model_path = Path(args.model)
    if not model_path.exists():
        raise FileNotFoundError(f"Model not found: {model_path}")

    capture = cv2.VideoCapture(args.camera)
    if not capture.isOpened():
        raise RuntimeError(f"Could not open camera index {args.camera}")

    base_options = python.BaseOptions(model_asset_path=str(model_path))
    options = vision.ObjectDetectorOptions(
        base_options=base_options,
        running_mode=vision.RunningMode.VIDEO,
        max_results=args.max_results,
        score_threshold=args.score,
    )

    frame_index = 0
    fps_clock = time.time()
    fps_counter = 0
    shown_fps = 0.0

    with vision.ObjectDetector.create_from_options(options) as detector:
        while True:
            ok, frame_bgr = capture.read()
            if not ok:
                break

            frame_rgb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=frame_rgb)
            timestamp_ms = int(frame_index * 1000 / max(capture.get(cv2.CAP_PROP_FPS), 1))
            detection_result = detector.detect_for_video(mp_image, timestamp_ms)

            draw_detection_result(frame_bgr, detection_result)

            fps_counter += 1
            now = time.time()
            if now - fps_clock >= 1.0:
                shown_fps = fps_counter / (now - fps_clock)
                fps_clock = now
                fps_counter = 0

            cv2.putText(
                frame_bgr,
                f"FPS {shown_fps:.1f}",
                (10, 28),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.75,
                (30, 220, 220),
                2,
                cv2.LINE_AA,
            )

            cv2.imshow("MediaPipe Object Detector", frame_bgr)
            key = cv2.waitKey(1) & 0xFF
            if key == 27 or key == ord("q"):
                break

            frame_index += 1

    capture.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
