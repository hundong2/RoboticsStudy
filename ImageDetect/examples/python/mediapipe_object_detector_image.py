import argparse
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
        score = category.score
        label = f"{name} {score:.2f}"

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
    parser = argparse.ArgumentParser(description="MediaPipe object detection for a still image.")
    parser.add_argument("--image", default="assets/images/coco_000000039769.jpg", help="Input image path.")
    parser.add_argument("--model", default="assets/models/efficientdet_lite0_int8.tflite", help="TFLite model path.")
    parser.add_argument("--output", default="outputs/mediapipe_result.jpg", help="Output image path.")
    parser.add_argument("--score", type=float, default=0.3, help="Score threshold.")
    parser.add_argument("--max-results", type=int, default=10, help="Maximum number of detections.")
    return parser.parse_args()


def main():
    args = parse_args()
    image_path = Path(args.image)
    model_path = Path(args.model)
    output_path = Path(args.output)

    if not image_path.exists():
        raise FileNotFoundError(f"Image not found: {image_path}")
    if not model_path.exists():
        raise FileNotFoundError(f"Model not found: {model_path}")

    base_options = python.BaseOptions(model_asset_path=str(model_path))
    options = vision.ObjectDetectorOptions(
        base_options=base_options,
        running_mode=vision.RunningMode.IMAGE,
        max_results=args.max_results,
        score_threshold=args.score,
    )

    mp_image = mp.Image.create_from_file(str(image_path))
    image_bgr = cv2.imread(str(image_path))
    if image_bgr is None:
        raise RuntimeError(f"OpenCV failed to read image: {image_path}")

    with vision.ObjectDetector.create_from_options(options) as detector:
        detection_result = detector.detect(mp_image)

    draw_detection_result(image_bgr, detection_result)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    cv2.imwrite(str(output_path), image_bgr)

    print(f"detections: {len(detection_result.detections)}")
    print(f"saved: {output_path}")


if __name__ == "__main__":
    main()
