"""
Nav2 failure debugging checklist.

Run:
    python slam_nav2/05_nav2_failure_debug_checklist.py
"""

from __future__ import annotations


CHECKS = [
    ("No map in RViz", ["map_server active?", "map yaml path correct?", "use_sim_time consistent?"]),
    ("Robot pose jumps", ["AMCL/SLAM localization unstable?", "odom covariance too optimistic?", "TF loop or duplicate publisher?"]),
    ("No global path", ["goal inside obstacle?", "global costmap receiving map?", "robot footprint too large?"]),
    ("No local motion", ["controller server active?", "/cmd_vel published?", "local costmap receiving obstacle data?"]),
    ("Obstacle ignored", ["LiDAR topic name correct?", "sensor frame has TF to base_link?", "obstacle layer enabled?"]),
    ("Recovery repeats forever", ["initial pose wrong?", "blocked corridor?", "behavior tree timeout too short?"]),
]


def main() -> None:
    print("Nav2 debugging checklist\n")
    for symptom, checks in CHECKS:
        print(symptom)
        for check in checks:
            print(f"  - {check}")
        print()
    print("Useful commands")
    print("  ros2 topic list")
    print("  ros2 topic hz /scan")
    print("  ros2 topic echo /cmd_vel --once")
    print("  ros2 run tf2_tools view_frames")
    print("  ros2 lifecycle nodes")


if __name__ == "__main__":
    main()
