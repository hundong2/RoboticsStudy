"""
Basic robotics platform check.

Run:
    python shared/00_check_platform.py

This script does not change the system. It prints useful facts for Jetson,
Raspberry Pi, ROS 2, camera, GPU, and Python package readiness.
"""

from __future__ import annotations

import importlib.util
import os
import platform
import shutil
import subprocess
from pathlib import Path


def command_output(command: list[str]) -> str:
    try:
        result = subprocess.run(command, check=False, text=True, capture_output=True, timeout=5)
    except Exception as exc:  # pragma: no cover - diagnostic script
        return f"ERROR: {exc}"
    text = result.stdout.strip() or result.stderr.strip()
    return text if text else "(no output)"


def has_module(name: str) -> bool:
    return importlib.util.find_spec(name) is not None


def print_section(title: str) -> None:
    print(f"\n== {title} ==")


def detect_board() -> str:
    model_paths = [
        Path("/proc/device-tree/model"),
        Path("/sys/firmware/devicetree/base/model"),
    ]
    for path in model_paths:
        if path.exists():
            try:
                return path.read_text(errors="ignore").replace("\x00", "").strip()
            except OSError:
                pass
    return "unknown"


def main() -> None:
    print_section("System")
    print("OS:", platform.platform())
    print("Machine:", platform.machine())
    print("Python:", platform.python_version())
    print("Board:", detect_board())

    print_section("NVIDIA Jetson hints")
    print("tegrastats:", shutil.which("tegrastats") or "not found")
    if Path("/etc/nv_tegra_release").exists():
        print("/etc/nv_tegra_release:", Path("/etc/nv_tegra_release").read_text(errors="ignore").strip())
    print("nvidia-smi:", shutil.which("nvidia-smi") or "not expected on many Jetson images")

    print_section("Raspberry Pi hints")
    print("libcamera-hello:", shutil.which("libcamera-hello") or "not found")
    print("rpicam-hello:", shutil.which("rpicam-hello") or "not found")

    print_section("ROS 2")
    print("ROS_DISTRO:", os.environ.get("ROS_DISTRO", "not sourced"))
    print("ros2:", shutil.which("ros2") or "not found")
    if shutil.which("ros2"):
        print(command_output(["ros2", "--help"]).splitlines()[0])

    print_section("Python packages")
    for package in ["cv2", "numpy", "rclpy", "picamera2", "ultralytics", "requests"]:
        print(f"{package:12s}", "OK" if has_module(package) else "missing")

    print_section("Camera quick notes")
    print("Jetson CSI camera: test with jetson_orin/01_jetson_camera_preview.py")
    print("USB camera: test with shared/01_opencv_camera_preview.py --source 0")
    print("Raspberry Pi camera: test with raspberry_pi/01_picamera2_preview.py")


if __name__ == "__main__":
    main()
