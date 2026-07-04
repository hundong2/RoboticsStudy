"""
SLAM/Nav2 concept guide.

Run:
    python slam_nav2/00_slam_concepts.py
"""

from __future__ import annotations


def main() -> None:
    print("SLAM/Nav2 minimum concepts")
    print("- /scan: 2D LiDAR LaserScan topic used by many ROS 2 SLAM examples")
    print("- /odom: short-term odometry frame, usually from wheel encoder/IMU/visual odometry")
    print("- map -> odom -> base_link: core TF chain for navigation")
    print("- slam_toolbox: common ROS 2 package for 2D SLAM")
    print("- map_saver: saves occupancy grid maps for later localization/navigation")
    print("- costmap: obstacle and inflation map used by Nav2 planners/controllers")
    print("- behavior tree: Nav2 task orchestration model")
    print("\nCamera-only note:")
    print("A camera module alone is not enough for standard 2D Nav2 SLAM unless you add depth/visual odometry.")
    print("Start with TurtleBot3 simulation or add a 2D LiDAR for the clearest SLAM path.")


if __name__ == "__main__":
    main()
