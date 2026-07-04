"""
TF2 coordinate transform demo without ROS 2 dependencies.

Why this matters:
    A detector returns an object in camera coordinates, but MoveIt2/Nav2 need
    the target in robot/world coordinates. TF2 is the ROS 2 tool that manages
    this transform chain. This file shows the math behind the idea.

Run:
    python shared/07_tf2_coordinate_transform_demo.py
"""

from __future__ import annotations

import math


def transform_2d(point: tuple[float, float], translation: tuple[float, float], yaw_deg: float) -> tuple[float, float]:
    x, y = point
    tx, ty = translation
    yaw = math.radians(yaw_deg)
    c, s = math.cos(yaw), math.sin(yaw)
    return (c * x - s * y + tx, s * x + c * y + ty)


def main() -> None:
    print("Scenario")
    print("- Camera sees a cup at (0.40m forward, 0.10m left) in camera frame.")
    print("- Camera is mounted 0.20m in front of base_link and rotated 15 degrees.")
    print("- We need the cup pose in base_link for robot motion planning.\n")

    cup_in_camera = (0.40, 0.10)
    camera_in_base_translation = (0.20, 0.00)
    camera_in_base_yaw_deg = 15.0
    cup_in_base = transform_2d(cup_in_camera, camera_in_base_translation, camera_in_base_yaw_deg)

    print("cup_in_camera:", cup_in_camera)
    print("camera_in_base translation:", camera_in_base_translation)
    print("camera_in_base yaw:", camera_in_base_yaw_deg)
    print("cup_in_base_link:", tuple(round(v, 3) for v in cup_in_base))
    print("\nROS 2 equivalent")
    print("- Publish camera_link relative to base_link with robot_state_publisher/static_transform_publisher.")
    print("- Query transform using tf2_ros Buffer.")
    print("- Convert detection pose before sending it to MoveIt2/Nav2.")


if __name__ == "__main__":
    main()
