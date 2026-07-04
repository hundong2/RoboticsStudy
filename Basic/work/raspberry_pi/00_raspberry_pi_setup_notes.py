"""
Raspberry Pi + Camera Module setup notes.

Run:
    python raspberry_pi/00_raspberry_pi_setup_notes.py
"""

from __future__ import annotations


def main() -> None:
    print("Raspberry Pi setup checklist")
    print("1. Install Raspberry Pi OS 64-bit when possible.")
    print("2. Update packages: sudo apt update && sudo apt upgrade")
    print("3. Test camera: rpicam-hello or libcamera-hello")
    print("4. Install Picamera2: sudo apt install python3-picamera2")
    print("5. Run: python raspberry_pi/01_picamera2_preview.py")
    print("6. For ROS 2, prefer Ubuntu Server on Pi if you want a standard ROS 2 install.")
    print("7. For SLAM, add a 2D LiDAR or use simulation on a PC/Jetson.")
    print("\nOfficial docs:")
    print("- https://www.raspberrypi.com/documentation/accessories/camera.html")
    print("- https://pip.raspberrypi.com/documents/RP-008156-DS-picamera2-manual.pdf")


if __name__ == "__main__":
    main()
