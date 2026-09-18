"""세 프로세스로 목표 발행, 궤적 계획, 독립 감사를 실행한다."""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(package='daily_robotics_2026_09_19', executable='goal_source',
             output='screen'),
        Node(package='daily_robotics_2026_09_19', executable='jerk_limited_planner',
             output='screen'),
        Node(package='daily_robotics_2026_09_19', executable='trajectory_auditor',
             output='screen'),
    ])
