# ROS 학습 로드맵

이 폴더는 ROS를 처음 접하는 사람이 ROS 2 기반 로봇 소프트웨어를 직접 만들고, 디버깅하고, 배포 관점까지 설명할 수 있게 만드는 실습형 학습 자료입니다.

기준일: 2026-07-12  
권장 주 배포판: ROS 2 Lyrical Luth LTS  
호환 학습 배포판: ROS 2 Jazzy Jalisco LTS, Kilted Kaiju, Rolling Ridley

## 학습 완료 기준

초보자는 단순히 명령어를 따라 치는 단계에서 멈추기 쉽습니다. 이 커리큘럼의 완료 기준은 다음 5가지를 모두 설명하고 재현하는 것입니다.

- ROS 그래프에서 Node, Topic, Service, Action, Parameter, TF가 어떤 책임을 갖는지 설명한다.
- `ros2` CLI, `rqt_graph`, RViz, rosbag2, 로그를 이용해 통신 문제를 추적한다.
- Python ROS 2 패키지를 만들고 publisher, subscriber, parameter, launch를 작성한다.
- URDF/TF, Gazebo, Nav2, MoveIt2, perception, edge AI를 하나의 로봇 시스템으로 연결한다.
- 최신 ROS 2 트렌드인 Lyrical LTS, Zenoh, zero-copy GPU 데이터 경로, asyncio rclpy, Physical AI, 시뮬레이션 기반 검증을 기술적으로 비교한다.

## 한눈에 보는 커리큘럼

| 단계 | 수준 | 핵심 목표 | 학습 문서 | 실습 프로젝트 | 통과 기준 |
|---|---|---|---|---|---|
| 0 | 준비 | Ubuntu/WSL2, Git, Python, C++, VS Code, Docker 준비 | [00_environment_setup.md](docs/00_environment_setup.md) | 환경 체크리스트 | `ros2 doctor`와 셸 환경을 설명 |
| 1 | 입문 | ROS가 OS가 아니라 로봇 미들웨어임을 이해 | [01_ros_concepts.md](docs/01_ros_concepts.md) | 개념 카드 작성 | Node/Topic/Service 차이 설명 |
| 2 | 기초 | CLI로 ROS 그래프 관찰 | [02_cli_graph_debugging.md](docs/02_cli_graph_debugging.md) | [01_turtlesim_cli_lab](projects/01_turtlesim_cli_lab/README.md) | turtlesim 이동과 topic echo 재현 |
| 3 | 기초 | Python 패키지, 노드, 타이머, 메시지 작성 | [03_python_package_nodes.md](docs/03_python_package_nodes.md) | [02_python_nodes_ws](projects/02_python_nodes_ws/README.md) | publisher/subscriber 실행 |
| 4 | 중급 | message, service, action, parameter 설계 | [04_messages_services_actions.md](docs/04_messages_services_actions.md) | Python 노드 확장 | 동기/비동기 통신 선택 근거 설명 |
| 5 | 중급 | TF2, URDF, xacro, RViz 모델링 | [05_tf2_urdf_robot_modeling.md](docs/05_tf2_urdf_robot_modeling.md) | [03_tf_urdf_robot_model](projects/03_tf_urdf_robot_model/README.md) | `map -> odom -> base_link` 흐름 설명 |
| 6 | 중급 | Gazebo 시뮬레이션과 센서 플러그인 | [06_gazebo_simulation.md](docs/06_gazebo_simulation.md) | Gazebo 로봇 스폰 | 센서 topic과 시뮬레이션 시간 구분 |
| 6-1 | 중급 | 실제 로봇 없이 무료 시뮬레이터로 실습 | [13_simulation_without_robot.md](docs/13_simulation_without_robot.md) | [06_robotless_simulation_lab](projects/06_robotless_simulation_lab/README.md) | Gazebo/Webots/Isaac/MuJoCo/CARLA 선택 근거 설명 |
| 7 | 고급 | SLAM, localization, Nav2, costmap | [07_nav2_slam_localization.md](docs/07_nav2_slam_localization.md) | [04_nav2_simulation_mission](projects/04_nav2_simulation_mission/README.md) | 맵 생성, 저장, 자율주행 목표 지정 |
| 8 | 고급 | MoveIt2, ros2_control, 조작 로봇 | [08_moveit2_manipulation.md](docs/08_moveit2_manipulation.md) | Pick and place 설계 | planning scene과 controller 연결 설명 |
| 9 | 고급 | 카메라, LiDAR, AI perception, edge 배포 | [09_perception_ai_edge.md](docs/09_perception_ai_edge.md) | 이미지 topic 분석 | latency와 throughput 측정 |
| 10 | 전문가 | 보안, 실시간성, 관측성, CI/CD | [10_security_realtime_operations.md](docs/10_security_realtime_operations.md) | rosbag 기반 회귀 테스트 | 실패 재현 로그와 테스트 작성 |
| 11 | 전문가 | 2026 최신 트렌드와 아키텍처 의사결정 | [11_latest_trends_2026.md](docs/11_latest_trends_2026.md) | 기술 선택 보고서 | Lyrical/Jazzy/Kilted/Rolling 선택 근거 |
| 12 | 전문가 | 통합 캡스톤으로 실력 검증 | [12_capstone_assessment.md](docs/12_capstone_assessment.md) | [05_capstone_autonomous_inspection](projects/05_capstone_autonomous_inspection/README.md) | 요구사항, 테스트, 데모 리포트 제출 |

## 권장 학습 루틴

| 기간 | 목표 | 산출물 |
|---|---|---|
| 1-2주 | ROS 2 설치, CLI, 그래프 개념 | `ros2 topic list`, `ros2 node info` 결과 캡처 |
| 3-4주 | Python 노드와 launch 작성 | `hello_robot_py` 패키지 실행 로그 |
| 5-6주 | TF/URDF/RViz/Gazebo | 차동 구동 로봇 모델과 TF 트리 |
| 7-8주 | SLAM/Nav2 | 맵 파일, 목표 지점 이동 로그 |
| 9-10주 | perception/MoveIt2/edge | 이미지 처리 노드 또는 조작 planning 데모 |
| 11-12주 | 운영/보안/실시간성/최신 트렌드 | 캡스톤 아키텍처 문서와 검증 리포트 |

## 실습 프로젝트 구조

- [projects/01_turtlesim_cli_lab](projects/01_turtlesim_cli_lab/README.md): ROS 2 CLI와 그래프 디버깅 첫 실습입니다.
- [projects/02_python_nodes_ws](projects/02_python_nodes_ws/README.md): 주석이 매우 많은 Python ROS 2 패키지 예제입니다.
- [projects/03_tf_urdf_robot_model](projects/03_tf_urdf_robot_model/README.md): TF2/URDF/xacro/RViz 학습용 차동 구동 로봇 모델입니다.
- [projects/04_nav2_simulation_mission](projects/04_nav2_simulation_mission/README.md): SLAM, localization, Nav2를 연결하는 미션 문서입니다.
- [projects/05_capstone_autonomous_inspection](projects/05_capstone_autonomous_inspection/README.md): 전문가 수준 검증을 위한 통합 프로젝트입니다.
- [projects/06_robotless_simulation_lab](projects/06_robotless_simulation_lab/README.md): 실제 로봇 없이 Gazebo, Webots, Isaac Sim, MuJoCo, PyBullet, Drake, CARLA를 선택해 실습하는 가이드입니다.

## 딥러닝 로봇 정책 학습 심화 트랙

채용 요건에 맞춘 연구형 커리큘럼은 [robot_learning/README.md](robot_learning/README.md)에 정리했습니다.

- [00_foundations](robot_learning/00_foundations/README.md): Python, tensor, 확률, 좌표계, 로봇 데이터 기초
- [01_pytorch_pipeline](robot_learning/01_pytorch_pipeline/README.md): PyTorch Dataset/DataLoader/model/loss/checkpoint 학습 파이프라인
- [02_policy_learning_il_bc](robot_learning/02_policy_learning_il_bc/README.md): Imitation Learning, Behavior Cloning, ACT, Diffusion Policy, VQ-BeT
- [03_rl_offline_rl](robot_learning/03_rl_offline_rl/README.md): Reinforcement Learning, Offline RL, dataset shift, conservative learning
- [04_generative_models](robot_learning/04_generative_models/README.md): VAE, diffusion, flow matching, action generation
- [05_visual_navigation_memory](robot_learning/05_visual_navigation_memory/README.md): Visual Navigation, Long-Horizon/Mapless Navigation, Topological Map/Memory
- [06_vla_world_models_generative_sim](robot_learning/06_vla_world_models_generative_sim/README.md): VLA, World Model, 3D Gaussian Splatting, 생성형 시뮬레이션
- [07_simulators_closed_loop](robot_learning/07_simulators_closed_loop/README.md): CARLA, Isaac Sim, Gazebo/Webots closed-loop 평가
- [08_sim_to_real_deployment](robot_learning/08_sim_to_real_deployment/README.md): 실제 로봇 배포와 sim-to-real gap 대응
- [09_on_device_inference](robot_learning/09_on_device_inference/README.md): ONNX Runtime, TensorRT, 온디바이스 latency 최적화
- [10_research_publication](robot_learning/10_research_publication/README.md): CVPR/ICCV/ECCV/NeurIPS/CoRL/ICRA/IROS 논문 읽기와 연구 설계
- [11_capstone](robot_learning/11_capstone/README.md): Visual Navigation Policy 캡스톤 연구 프로젝트
- [daily_practice](robot_learning/daily_practice/README.md): 매일 실습 운영표와 12주 압축 루트

## AI 로봇 C++/ROS2 실무 파이프라인

로봇 장비가 없는 환경에서 C++ 머신러닝 추론 노드와 로봇 제어 파이프라인을 시뮬레이션으로 검증하고, 이후 실제 로봇에 안전하게 배포하는 실무 가이드는 [ai_robotics_cpp_pipeline/README.md](ai_robotics_cpp_pipeline/README.md)에 정리했습니다.

- [ROS 생태계 이론](ai_robotics_cpp_pipeline/docs/01_ros_ecosystem_theory.md): ROS 2 통신, 제어, 시뮬레이션, AI 계층의 큰 그림
- [C++ ML 로봇 파이프라인](ai_robotics_cpp_pipeline/docs/02_cpp_ml_robot_pipeline.md): PyTorch -> ONNX -> C++ ROS 2 추론 노드 흐름
- [로봇 없는 시뮬레이션 테스트](ai_robotics_cpp_pipeline/docs/03_no_robot_simulation_testing.md): Gazebo, Isaac Sim, PyBullet, MuJoCo 기반 closed-loop 테스트
- [LLM Agent 로봇 응용](ai_robotics_cpp_pipeline/docs/04_llm_agent_robot_apps.md): LangChain, LlamaIndex, RAG, 스크립트 자동생성의 안전 구조
- [실제 로봇 테스트 파이프라인](ai_robotics_cpp_pipeline/docs/05_real_robot_testing_pipeline.md): dry-run, 저속 테스트, safety filter, rollback
- [임베디드 추론 최적화](ai_robotics_cpp_pipeline/docs/06_embedded_optimization.md): ONNX Runtime, TensorRT, p99 latency 측정
- [용어 사전](ai_robotics_cpp_pipeline/glossary/README.md): 문서 안의 ROS/AI/시뮬레이터 용어를 사전 형태로 정리

## 검증 자료

- [검증 계획](docs/validation_plan.md)
- [3회 검증 리포트](docs/validation_report.md)
- [검증 스크립트](scripts/validate_ros_learning_materials.py)

## 공식 자료 기준

이 자료는 공식 ROS/OSRF 자료와 최신 릴리스 공지를 기준으로 구성했습니다.

- ROS 공식 사이트: <https://www.ros.org/>
- ROS 2 문서: <https://docs.ros.org/>
- ROS 2 릴리스/플랫폼 REP-2000: <https://reps.openrobotics.org/rep-2000/>
- ROS 2 Lyrical Luth 릴리스 공지: <https://discourse.openrobotics.org/t/ros-2-lyrical-luth-released/55021>
- ROS 2 릴리스 태그: <https://github.com/ros2/ros2/releases>
- Open Robotics Technology Strategy 2026: <https://osralliance.org/open-robotics-technology-strategy-for-2026/>
- Gazebo ROS 2 설치/연동: <https://gazebosim.org/docs/latest/ros_installation/>
- Webots ROS 2 패키지: <https://github.com/cyberbotics/webots_ros2>
- Isaac Sim ROS 2 Bridge: <https://docs.isaacsim.omniverse.nvidia.com/6.0.0/installation/install_ros.html>
- MuJoCo: <https://mujoco.org/>
- CARLA ROS bridge: <https://carla.readthedocs.io/projects/ros-bridge/en/latest/run_ros/>
- PyTorch Tutorials: <https://docs.pytorch.org/tutorials/>
- Hugging Face LeRobot: <https://huggingface.co/docs/lerobot/en/index>
- Diffusion Policy: <https://arxiv.org/abs/2303.04137>
- ViNT: <https://arxiv.org/abs/2306.14846>
- NoMaD: <https://arxiv.org/abs/2310.07896>
- LangChain Agents: <https://docs.langchain.com/oss/python/langchain/agents>
- LlamaIndex: <https://www.llamaindex.ai/>
- ONNX Runtime: <https://onnxruntime.ai/docs/>
- NVIDIA TensorRT: <https://docs.nvidia.com/deeplearning/tensorrt/latest/index.html>
