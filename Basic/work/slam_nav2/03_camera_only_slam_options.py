"""
Camera-only SLAM options.

A single camera module is useful, but standard Nav2 2D mapping examples expect
LaserScan. This guide explains realistic options.
"""

from __future__ import annotations


def main() -> None:
    print("Camera-only SLAM paths")
    print("1. Monocular Visual SLAM")
    print("   - Example family: ORB-SLAM")
    print("   - Pros: works with one camera")
    print("   - Cons: scale ambiguity, harder ROS2/Nav2 integration")
    print("2. RGB-D SLAM")
    print("   - Requires depth camera, not a normal Pi camera module")
    print("   - Example family: RTAB-Map")
    print("3. Visual odometry + Nav2")
    print("   - Use camera for odometry, but still need obstacle source for costmap")
    print("4. Recommended beginner path")
    print("   - First use TurtleBot3 simulation")
    print("   - Then add a 2D LiDAR to Jetson/Raspberry Pi robot")
    print("   - Use the camera for object detection, not first SLAM")


if __name__ == "__main__":
    main()
