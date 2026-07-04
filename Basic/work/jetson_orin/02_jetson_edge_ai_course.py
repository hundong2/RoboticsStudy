"""
Jetson edge AI course checklist mapped to Basic notebooks.

Run:
    python jetson_orin/02_jetson_edge_ai_course.py
"""

from __future__ import annotations


COURSE = [
    ("ROS2 기초", "shared/05_ros2_camera_node_guide.py", "Node/Topic/Service/Action/Launch/TF2를 확인"),
    ("비전", "shared/01_opencv_camera_preview.py, shared/03_yolo_camera_infer.py", "카메라 입력과 YOLO 추론"),
    ("SLAM/Nav2", "slam_nav2/*.py", "LiDAR 또는 TurtleBot3 시뮬레이션으로 map 생성"),
    ("LLM/Ollama", "shared/04_ollama_command_parser.py", "자연어 명령을 action sequence로 변환"),
    ("MVP", "../11_mvp_integration_project.ipynb", "Vision + ROS2 + Nav2/MoveIt2 + LLM 연결"),
]


def main() -> None:
    for index, (title, file_hint, goal) in enumerate(COURSE, start=1):
        print(f"{index}. {title}")
        print(f"   file: {file_hint}")
        print(f"   goal: {goal}")


if __name__ == "__main__":
    main()
