"""Show OpenCV DNN build/runtime information.

This is a lightweight companion for DNN/OpenVINO deployment study. If your
OpenCV build has OpenVINO support, it appears in the build information.

Run:
    uv run python play/opencv_dnn_info.py
"""

from __future__ import annotations

import cv2


def main() -> None:
    print("OpenCV version:", cv2.__version__)
    print("Available DNN backend constants:")
    for name in sorted(dir(cv2.dnn)):
        if name.startswith("DNN_BACKEND_") or name.startswith("DNN_TARGET_"):
            print(" ", name, "=", getattr(cv2.dnn, name))

    print()
    print("Build information lines mentioning OpenVINO/Inference Engine:")
    lines = cv2.getBuildInformation().splitlines()
    matches = [
        line
        for line in lines
        if "openvino" in line.lower() or "inference engine" in line.lower()
    ]
    if matches:
        for line in matches:
            print(line)
    else:
        print("No OpenVINO-specific build line found in this opencv-python wheel.")


if __name__ == "__main__":
    main()
