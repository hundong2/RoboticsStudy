"""
ROS 2 camera node guide.

This script prints a minimal rclpy node skeleton and the commands needed to
publish camera images. It avoids importing ROS 2 unless you ask for the check.

Run:
    python shared/05_ros2_camera_node_guide.py
    python shared/05_ros2_camera_node_guide.py --check
"""

from __future__ import annotations

import argparse
import importlib.util
import os


SKELETON = r'''
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2

class CameraNode(Node):
    def __init__(self):
        super().__init__("basic_camera_node")
        self.pub = self.create_publisher(Image, "/camera/image_raw", 10)
        self.bridge = CvBridge()
        self.cap = cv2.VideoCapture(0)
        self.timer = self.create_timer(1 / 30, self.tick)

    def tick(self):
        ok, frame = self.cap.read()
        if ok:
            self.pub.publish(self.bridge.cv2_to_imgmsg(frame, encoding="bgr8"))

def main():
    rclpy.init()
    node = CameraNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    print("ROS_DISTRO:", os.environ.get("ROS_DISTRO", "not sourced"))
    print("\nInstall hints:")
    print("  sudo apt install ros-$ROS_DISTRO-cv-bridge ros-$ROS_DISTRO-image-transport")
    print("  source /opt/ros/$ROS_DISTRO/setup.bash")
    print("\nUseful commands:")
    print("  ros2 topic list")
    print("  ros2 topic echo /camera/camera_info --once")
    print("  ros2 run rqt_image_view rqt_image_view")
    print("\nMinimal node skeleton:")
    print(SKELETON)

    if args.check:
        for module in ["rclpy", "sensor_msgs", "cv_bridge", "cv2"]:
            print(f"{module:12s}", "OK" if importlib.util.find_spec(module) else "missing")


if __name__ == "__main__":
    main()
