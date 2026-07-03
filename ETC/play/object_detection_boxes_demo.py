"""Compare object-detection box ideas used by SSD, YOLO, and Faster R-CNN.

This script does not run a trained detector. It shows the geometry that all
detectors must eventually solve: candidate boxes, confidence, IoU, and NMS.

Run:
    uv run python play/object_detection_boxes_demo.py
"""

from __future__ import annotations

import numpy as np


def iou(box: np.ndarray, boxes: np.ndarray) -> np.ndarray:
    x1 = np.maximum(box[0], boxes[:, 0])
    y1 = np.maximum(box[1], boxes[:, 1])
    x2 = np.minimum(box[2], boxes[:, 2])
    y2 = np.minimum(box[3], boxes[:, 3])

    intersection = np.maximum(0, x2 - x1) * np.maximum(0, y2 - y1)
    box_area = (box[2] - box[0]) * (box[3] - box[1])
    boxes_area = (boxes[:, 2] - boxes[:, 0]) * (boxes[:, 3] - boxes[:, 1])
    return intersection / np.maximum(box_area + boxes_area - intersection, 1e-6)


def nms(boxes: np.ndarray, scores: np.ndarray, threshold: float = 0.5) -> list[int]:
    order = scores.argsort()[::-1]
    keep: list[int] = []

    while order.size:
        current = int(order[0])
        keep.append(current)
        if order.size == 1:
            break
        overlaps = iou(boxes[current], boxes[order[1:]])
        order = order[1:][overlaps < threshold]

    return keep


def main() -> None:
    boxes = np.array(
        [
            [40, 40, 160, 160],
            [50, 48, 170, 168],
            [220, 50, 330, 180],
            [225, 60, 335, 188],
            [20, 220, 120, 330],
        ],
        dtype=np.float32,
    )
    scores = np.array([0.95, 0.87, 0.91, 0.72, 0.66], dtype=np.float32)

    keep = nms(boxes, scores, threshold=0.45)

    print("Candidate box sources:")
    print("- SSD: default/anchor boxes on multi-scale feature maps")
    print("- YOLO: grid or anchor-free predictions in one forward pass")
    print("- Faster R-CNN: region proposal network candidates")
    print()
    print("boxes:")
    for idx, (box, score) in enumerate(zip(boxes, scores)):
        print(f"  {idx}: {box.astype(int).tolist()} score={score:.2f}")
    print("kept after NMS:", keep)


if __name__ == "__main__":
    main()
