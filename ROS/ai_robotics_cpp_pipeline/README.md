# AI 로봇 C++/ROS2 실무 파이프라인 가이드

이 문서는 로봇 기기가 없는 환경에서 [C++](glossary/README.md#cpp), [Python](glossary/README.md#python), [ROS 2](glossary/README.md#ros2), [머신러닝 추론](glossary/README.md#inference), [LLM Agent](glossary/README.md#llm-agent), [시뮬레이터](glossary/README.md#simulator), [임베디드 최적화](glossary/README.md#embedded-optimization)를 연결해 학습하고, 이후 실제 로봇에서 안전하게 테스트하는 전체 파이프라인을 설명합니다.

기준일: 2026-08-07

## 한눈에 보는 전체 파이프라인

```text
1. Python/PyTorch에서 정책 또는 인식 모델 학습
   -> 2. ONNX로 export
   -> 3. C++ ROS 2 추론 노드 작성
   -> 4. Gazebo/Isaac Sim/PyBullet/MuJoCo에서 무로봇 closed-loop 테스트
   -> 5. rosbag2와 launch_testing으로 반복 검증
   -> 6. safety filter와 lifecycle node로 실제 로봇 dry-run
   -> 7. 저속 제한 실제 주행
   -> 8. ONNX Runtime/TensorRT로 온디바이스 최적화
   -> 9. 로그와 실패 사례를 다시 학습 데이터로 반영
```

## 목차

| 순서 | 문서 | 목적 |
|---:|---|---|
| 1 | [ROS 생태계 이론](docs/01_ros_ecosystem_theory.md) | [노드](glossary/README.md#node), [토픽](glossary/README.md#topic), [서비스](glossary/README.md#service), [액션](glossary/README.md#action), [TF](glossary/README.md#tf), [ros2_control](glossary/README.md#ros2-control)을 큰 그림으로 이해 |
| 2 | [C++ ML 로봇 파이프라인](docs/02_cpp_ml_robot_pipeline.md) | 학습 모델을 [ONNX](glossary/README.md#onnx)로 내보내고 [C++ ROS 2 노드](glossary/README.md#rclcpp)에서 실행 |
| 3 | [로봇 없는 시뮬레이션 테스트](docs/03_no_robot_simulation_testing.md) | [Gazebo](glossary/README.md#gazebo), [Isaac Sim](glossary/README.md#isaac-sim), [PyBullet](glossary/README.md#pybullet), [MuJoCo](glossary/README.md#mujoco)로 테스트 |
| 4 | [LLM Agent 로봇 응용](docs/04_llm_agent_robot_apps.md) | [LangChain](glossary/README.md#langchain), [LlamaIndex](glossary/README.md#llamaindex), [RAG](glossary/README.md#rag), 스크립트 자동생성 |
| 5 | [실제 로봇 테스트 파이프라인](docs/05_real_robot_testing_pipeline.md) | dry-run, 저속 주행, safety filter, rollback, [sim-to-real](glossary/README.md#sim-to-real) |
| 6 | [임베디드 추론 최적화](docs/06_embedded_optimization.md) | [ONNX Runtime](glossary/README.md#onnx-runtime), [TensorRT](glossary/README.md#tensorrt), latency 측정 |
| 7 | [데일리 실습 커리큘럼](docs/07_daily_lab_curriculum.md) | 초보자에서 실무 투입 수준까지 8주 루틴 |
| 8 | [용어 사전](glossary/README.md) | 문서 안의 기술 용어를 사전처럼 검색 |
| 9 | [검증 리포트](docs/08_validation_report.md) | 3회 테스트와 개선 내역 |

## 코드 예제

- [C++ ML Policy Node 예제](examples/cpp_ml_policy_node/README.md)
- [ml_policy_node.cpp](examples/cpp_ml_policy_node/src/ml_policy_node.cpp): `/scan`을 받아 `/cmd_vel`을 내보내는 C++ ROS 2 정책 노드입니다.
- [safety_filter.cpp](examples/cpp_ml_policy_node/src/safety_filter.cpp): 정책 출력 앞에 붙이는 안전 필터 예제입니다.
- [agent_command_bridge.py](examples/python_agent_bridge/agent_command_bridge.py): LLM Agent가 만든 명령을 바로 모터 명령으로 보내지 않고 안전한 JSON 계획으로 변환하는 예제입니다.

## 학습 완료 기준

- [ ] [ROS 2](glossary/README.md#ros2) 생태계를 통신, 제어, 시뮬레이션, 배포 관점으로 설명한다.
- [ ] [PyTorch](glossary/README.md#pytorch) 모델을 [ONNX](glossary/README.md#onnx)로 export하고 C++에서 추론하는 흐름을 설명한다.
- [ ] 로봇 없이 [Gazebo](glossary/README.md#gazebo) 또는 [Isaac Sim](glossary/README.md#isaac-sim)에서 [closed-loop](glossary/README.md#closed-loop) 테스트를 설계한다.
- [ ] [LLM Agent](glossary/README.md#llm-agent)가 로봇을 직접 제어하지 않고 검증 가능한 계획을 생성해야 하는 이유를 설명한다.
- [ ] 실제 로봇 테스트에서 [safety filter](glossary/README.md#safety-filter), [dry-run](glossary/README.md#dry-run), [rollback](glossary/README.md#rollback)을 적용한다.
- [ ] 온디바이스에서 p50/p90/p99 [latency](glossary/README.md#latency)를 측정하고 병목을 찾는다.

## 공식 자료 기준

- ROS 2 Concepts: <https://docs.ros.org/en/rolling/Concepts.html>
- ROS 2 C++ Publisher/Subscriber: <https://docs.ros.org/en/humble/Tutorials/Beginner-Client-Libraries/Writing-A-Simple-Cpp-Publisher-And-Subscriber.html>
- ros2_control: <https://control.ros.org/>
- ROS 2 launch testing: <https://docs.ros.org/en/rolling/Tutorials/Intermediate/Testing/Integration.html>
- Gazebo ROS 2 integration: <https://gazebosim.org/docs/latest/ros2_integration/>
- Isaac Sim ROS 2 Bridge: <https://docs.isaacsim.omniverse.nvidia.com/6.0.0/ros2_tutorials/ros2_landing_page.html>
- MuJoCo: <https://mujoco.org/>
- Bullet/PyBullet: <https://github.com/bulletphysics/bullet3>
- LangChain Agents: <https://docs.langchain.com/oss/python/langchain/agents>
- LlamaIndex: <https://www.llamaindex.ai/>
- PyTorch ONNX: <https://docs.pytorch.org/docs/stable/onnx.html>
- ONNX Runtime: <https://onnxruntime.ai/docs/>
- TensorRT: <https://docs.nvidia.com/deeplearning/tensorrt/latest/index.html>
