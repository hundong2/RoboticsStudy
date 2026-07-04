# Robotics Basic 학습 가이드

로봇 분야에 처음 진입할 때 용어가 한꺼번에 쏟아지는 문제를 줄이기 위한 입문 자료입니다. 노트북은 순서대로 읽도록 구성했고, 각 주제 중간에 필수 논문/문서 읽기 가이드를 연결했습니다.

## 추천 순서

1. [00_roadmap.ipynb](00_roadmap.ipynb) - 전체 지형도, 분야별 용어, 학습 순서
2. [07_learning_roadmap_map.ipynb](07_learning_roadmap_map.ipynb) - 전체 학습 흐름을 이미지 맵으로 보기
3. [12_full_course_deep_dive.ipynb](12_full_course_deep_dive.ipynb) - ROS2, SLAM/Nav2, MoveIt2, 모방학습, 비전/LLM을 하나의 시스템으로 깊게 이해
4. [13_edge_device_practical_labs.ipynb](13_edge_device_practical_labs.ipynb) - Jetson Orin Nano/Raspberry Pi/SLAM 실습 랩
5. [01_robotics_math.ipynb](01_robotics_math.ipynb) - 좌표계, 행렬, 확률, 최적화
6. [02_perception_sensors_cv.ipynb](02_perception_sensors_cv.ipynb) - 카메라, LiDAR, IMU, OpenCV, 딥러닝 인식
7. [03_slam_localization_mapping.ipynb](03_slam_localization_mapping.ipynb) - SLAM, localization, mapping, loop closure
8. [04_ros2_robot_systems.ipynb](04_ros2_robot_systems.ipynb) - ROS 2, 메시지, TF, Nav2, 실무 시스템 구조
9. [05_simulation_isaac_sim.ipynb](05_simulation_isaac_sim.ipynb) - Isaac Sim, Isaac Lab, synthetic data, sim-to-real
10. [06_vlm_vla_robot_learning.ipynb](06_vlm_vla_robot_learning.ipynb) - VLM, VLA, imitation learning, robot foundation model
11. [08_moveit2_manipulation.ipynb](08_moveit2_manipulation.ipynb) - MoveIt2, 모션 플래닝, Pick & Place
12. [09_omx_imitation_learning.ipynb](09_omx_imitation_learning.ipynb) - OMX 활용 텔레옵, 데이터 수집, 모방학습
13. [10_vision_llm_integration.ipynb](10_vision_llm_integration.ipynb) - OpenCV/YOLO, Ollama, 자연어 명령 연동
14. [11_mvp_integration_project.ipynb](11_mvp_integration_project.ipynb) - MVP 통합 프로젝트 설계

## 학습 품질 기준

이 자료는 단순 용어 암기가 아니라 아래 질문에 답할 수 있는 상태를 목표로 합니다.

- 입력은 무엇인가? 예: image, `/scan`, `/odom`, 자연어 명령
- 출력은 무엇인가? 예: detection box, occupancy grid, action sequence, joint trajectory
- 어떤 좌표계를 기준으로 하는가? 예: `camera_frame`, `base_link`, `map`
- 실패하면 어떤 로그와 topic을 먼저 확인해야 하는가?
- 시뮬레이션과 실제 보드에서 차이가 나는 지점은 무엇인가?

각 장을 읽은 뒤 이 다섯 가지를 설명할 수 없으면 다음 장으로 넘어가기보다 [12_full_course_deep_dive.ipynb](12_full_course_deep_dive.ipynb)와 [13_edge_device_practical_labs.ipynb](13_edge_device_practical_labs.ipynb)의 예제를 먼저 실행하세요.

## 보드 실습 가이드

Jetson Orin Nano Developer Kit, Raspberry Pi + Camera Module, SLAM/Nav2 실습용 Python 가이드는 [work/README.md](work/README.md)에 정리했습니다.

- [work/shared](work/shared) - 공통 환경 점검, OpenCV 카메라, 데이터 수집, YOLO, Ollama, ROS2 카메라 노드 가이드
- [work/jetson_orin](work/jetson_orin) - Jetson Orin Nano 카메라/Edge AI 실습 순서
- [work/raspberry_pi](work/raspberry_pi) - Raspberry Pi Camera Module/Picamera2 실습 순서
- [work/slam_nav2](work/slam_nav2) - TurtleBot3 시뮬레이션 또는 실제 로봇 SLAM/Nav2 가이드

## 논문/문서 가이드

- [papers/README.md](papers/README.md)
- [papers/probabilistic_robotics.md](papers/probabilistic_robotics.md)
- [papers/orb_slam.md](papers/orb_slam.md)
- [papers/attention_is_all_you_need.md](papers/attention_is_all_you_need.md)
- [papers/clip.md](papers/clip.md)
- [papers/palm_e.md](papers/palm_e.md)
- [papers/rt1.md](papers/rt1.md)
- [papers/rt2.md](papers/rt2.md)
- [papers/isaac_sim_docs.md](papers/isaac_sim_docs.md)

## 학습 기준

- 처음 2주는 "용어를 아는 상태"가 목표입니다. 구현까지 욕심내지 않아도 됩니다.
- 3-6주는 SLAM과 ROS 2의 연결을 이해하는 것이 목표입니다.
- 7주 이후부터 MoveIt2, Isaac Sim, VLM, VLA, LLM/Ollama를 실제 프로젝트 후보로 붙이면 됩니다.
- 로봇 AI는 수학, 센서, 시스템, 학습 모델이 섞인 분야입니다. 하나를 완벽히 끝내고 다음으로 가기보다, 얇게 한 바퀴 돌고 다시 깊게 들어가는 방식이 효율적입니다.

## 3-4주 이론 수업 묶음

사진 속 커리큘럼처럼 초반 이론 단계에서는 아래 항목을 하나의 묶음으로 보는 것이 좋습니다.

- ROS2 기초: Node, Topic, Service, Action, Launch, URDF/TF2
- SLAM/Nav2: 매핑 원리, 경로 계획, 코스트맵, 행동트리
- MoveIt2: 모션 플래닝, Pick & Place, 작업 시퀀스
- OMX 활용 모방학습: 텔레옵, 데이터, 학습, 추론
- 비전/LLM: OpenCV/YOLO, Ollama 기반 자연어 명령 해석
- 통합 마무리: MVP 인지/자연어 명령 연동
