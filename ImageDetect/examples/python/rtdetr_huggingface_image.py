import argparse
from pathlib import Path

import cv2
import numpy as np
import requests
import torch
from PIL import Image
from transformers import RTDetrForObjectDetection, RTDetrImageProcessor


def load_image(path_or_url):
    if path_or_url.startswith("http://") or path_or_url.startswith("https://"):
        return Image.open(requests.get(path_or_url, stream=True, timeout=30).raw).convert("RGB")
    return Image.open(path_or_url).convert("RGB")


def parse_args():
    parser = argparse.ArgumentParser(description="RT-DETR image detection with Hugging Face Transformers.")
    parser.add_argument("--image", default="assets/images/coco_000000039769.jpg", help="Input image path or URL.")
    parser.add_argument("--model-id", default="PekingU/rtdetr_r50vd", help="Hugging Face model id.")
    parser.add_argument("--output", default="outputs/rtdetr_result.jpg", help="Output image path.")
    parser.add_argument("--score", type=float, default=0.3, help="Score threshold.")
    return parser.parse_args()


def main():
    args = parse_args()
    image = load_image(args.image)

    device = "cuda" if torch.cuda.is_available() else "cpu"
    image_processor = RTDetrImageProcessor.from_pretrained(args.model_id)
    model = RTDetrForObjectDetection.from_pretrained(args.model_id).to(device)
    model.eval()

    inputs = image_processor(images=image, return_tensors="pt").to(device)
    with torch.no_grad():
        outputs = model(**inputs)

    target_sizes = torch.tensor([(image.height, image.width)], device=device)
    results = image_processor.post_process_object_detection(
        outputs,
        target_sizes=target_sizes,
        threshold=args.score,
    )[0]

    image_bgr = cv2.cvtColor(np.array(image), cv2.COLOR_RGB2BGR)
    for score, label_id, box in zip(results["scores"], results["labels"], results["boxes"]):
        x1, y1, x2, y2 = [int(v) for v in box.tolist()]
        label = model.config.id2label[int(label_id)]
        text = f"{label} {float(score):.2f}"
        cv2.rectangle(image_bgr, (x1, y1), (x2, y2), (40, 90, 230), 2)
        cv2.putText(
            image_bgr,
            text,
            (x1, max(20, y1 - 8)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.55,
            (40, 90, 230),
            2,
            cv2.LINE_AA,
        )
        print(text, [round(v, 1) for v in box.tolist()])

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    cv2.imwrite(str(output_path), image_bgr)
    print(f"saved: {output_path}")


if __name__ == "__main__":
    main()
