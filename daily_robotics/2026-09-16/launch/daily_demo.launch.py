"""두 센서, 온라인 시간 보정/융합기, 독립 auditor를 한 번에 실행한다."""

from launch import LaunchDescription
# launch_ros.actions.Node는 설치된 ROS 2 실행 파일을 각각 독립 process로 실행한다.
from launch_ros.actions import Node


def generate_launch_description():
    """동일 실행 파일을 IMU/LiDAR 파라미터로 두 번 실행해 clock offset을 재현한다."""
    return LaunchDescription(
        [
            Node(
                package="daily_robotics_2026_09_16",
                executable="yaw_rate_sensor",
                name="imu_yaw_rate_sensor",
                output="screen",
                parameters=[
                    {
                        "sensor_name": "imu",
                        "topic": "/imu/yaw_rate",
                        "frame_id": "imu_link",
                        "rate_hz": 200.0,
                        "stamp_offset_ms": 0.0,
                        "variance": 0.0004,
                    }
                ],
            ),
            Node(
                package="daily_robotics_2026_09_16",
                executable="yaw_rate_sensor",
                name="lidar_yaw_rate_sensor",
                output="screen",
                parameters=[
                    {
                        "sensor_name": "lidar",
                        "topic": "/lidar/yaw_rate",
                        "frame_id": "lidar_link",
                        "rate_hz": 20.0,
                        "stamp_offset_ms": 35.0,
                        "variance": 0.0025,
                    }
                ],
            ),
            Node(
                package="daily_robotics_2026_09_16",
                executable="time_offset_fusion",
                name="time_offset_fusion",
                output="screen",
            ),
            Node(
                package="daily_robotics_2026_09_16",
                executable="calibration_auditor",
                name="calibration_auditor",
                output="screen",
                parameters=[{"expected_offset_ms": 35.0}],
            ),
        ]
    )
