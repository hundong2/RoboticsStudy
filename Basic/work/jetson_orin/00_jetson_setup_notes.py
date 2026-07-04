"""
Jetson Orin Nano Developer Kit setup notes.

Run:
    python jetson_orin/00_jetson_setup_notes.py

Use NVIDIA's official quick start and JetPack release notes for flashing and
firmware. This script only prints a practical checklist.
"""

from __future__ import annotations


def main() -> None:
    print("Jetson Orin Nano setup checklist")
    print("1. Flash the SD card or storage using the official NVIDIA guide.")
    print("2. Boot once with monitor/keyboard or serial console and finish Ubuntu setup.")
    print("3. Confirm JetPack/L4T version: cat /etc/nv_tegra_release")
    print("4. Update packages: sudo apt update && sudo apt upgrade")
    print("5. Test camera:")
    print("   - CSI camera: python jetson_orin/01_jetson_camera_preview.py")
    print("   - USB camera: python shared/01_opencv_camera_preview.py --source 0")
    print("6. Test performance monitor: sudo tegrastats")
    print("7. For ROS 2, match Ubuntu version with the ROS 2 distro you install.")
    print("\nOfficial docs:")
    print("- https://docs.nvidia.com/jetson/orin-nano-devkit/user-guide/latest/quick_start.html")
    print("- https://developer.nvidia.com/embedded/jetpack/downloads")


if __name__ == "__main__":
    main()
