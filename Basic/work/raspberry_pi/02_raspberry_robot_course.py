"""
Raspberry Pi robotics course checklist.

Raspberry Pi is good for camera capture, lightweight ROS 2 nodes, sensor
bridges, and robot control. Heavy YOLO/VLM inference is usually better on
Jetson or a PC unless you use a small model or accelerator.
"""

from __future__ import annotations


def main() -> None:
    print("Raspberry Pi course path")
    print("1. Camera Module: raspberry_pi/01_picamera2_preview.py")
    print("2. Dataset capture: shared/02_capture_dataset.py --camera picamera2")
    print("3. Lightweight OpenCV: color/marker/object preprocessing")
    print("4. ROS 2 sensor bridge: publish camera/encoder/IMU topics")
    print("5. SLAM: use external 2D LiDAR + wheel odometry, or run SLAM on a stronger machine")
    print("6. LLM/YOLO: call Jetson/PC server over network if local performance is insufficient")


if __name__ == "__main__":
    main()
