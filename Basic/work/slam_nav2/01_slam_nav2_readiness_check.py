"""
Check whether the current machine looks ready for ROS 2 SLAM/Nav2.

Run after sourcing ROS 2:
    source /opt/ros/humble/setup.bash
    python slam_nav2/01_slam_nav2_readiness_check.py
"""

from __future__ import annotations

import os
import shutil
import subprocess


def run(command: list[str]) -> str:
    try:
        result = subprocess.run(command, check=False, text=True, capture_output=True, timeout=8)
    except Exception as exc:
        return f"ERROR: {exc}"
    return (result.stdout or result.stderr).strip()


def main() -> None:
    print("ROS_DISTRO:", os.environ.get("ROS_DISTRO", "not sourced"))
    print("ros2:", shutil.which("ros2") or "not found")
    if not shutil.which("ros2"):
        print("Install/source ROS 2 first.")
        return

    print("\nInstalled package checks:")
    packages = ["slam_toolbox", "nav2_bringup", "turtlebot3_gazebo", "rviz2"]
    for package in packages:
        output = run(["ros2", "pkg", "prefix", package])
        status = "OK" if output and not output.startswith("ERROR") and "not found" not in output.lower() else "missing"
        print(f"{package:18s} {status}")

    print("\nLive topic hints:")
    print(run(["ros2", "topic", "list"]))
    print("\nExpected for real robot SLAM:")
    print("- /scan from LiDAR")
    print("- /odom from odometry")
    print("- TF chain including base_link and laser frame")


if __name__ == "__main__":
    main()
