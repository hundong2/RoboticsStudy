"""
Print step-by-step commands for SLAM/Nav2 practice.

Simulation mode is recommended before using a real robot.

Run:
    python slam_nav2/02_slam_nav2_command_builder.py --ros-distro humble --mode turtlebot3
    python slam_nav2/02_slam_nav2_command_builder.py --ros-distro humble --mode real_robot
"""

from __future__ import annotations

import argparse


def turtlebot3_commands(distro: str) -> list[str]:
    return [
        f"source /opt/ros/{distro}/setup.bash",
        "export TURTLEBOT3_MODEL=burger",
        "ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py",
        "ros2 launch slam_toolbox online_async_launch.py use_sim_time:=true",
        "ros2 launch nav2_bringup navigation_launch.py use_sim_time:=true",
        "ros2 run nav2_map_server map_saver_cli -f maps/turtlebot3_map",
    ]


def real_robot_commands(distro: str) -> list[str]:
    return [
        f"source /opt/ros/{distro}/setup.bash",
        "# Bring up your robot drivers first: LiDAR, wheel odometry, robot_state_publisher",
        "ros2 topic list",
        "ros2 run tf2_tools view_frames",
        "ros2 launch slam_toolbox online_async_launch.py",
        "ros2 launch nav2_bringup navigation_launch.py",
        "ros2 run nav2_map_server map_saver_cli -f maps/real_robot_map",
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ros-distro", default="humble")
    parser.add_argument("--mode", choices=["turtlebot3", "real_robot"], default="turtlebot3")
    args = parser.parse_args()

    commands = turtlebot3_commands(args.ros_distro) if args.mode == "turtlebot3" else real_robot_commands(args.ros_distro)
    print(f"SLAM/Nav2 commands for mode={args.mode}")
    print("Run each command in an appropriate terminal, not all at once.\n")
    for index, command in enumerate(commands, start=1):
        print(f"{index}. {command}")


if __name__ == "__main__":
    main()
