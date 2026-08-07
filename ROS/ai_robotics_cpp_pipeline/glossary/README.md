# 용어 사전

이 사전은 문서 안에서 자주 나오는 단어를 빠르게 찾기 위한 색인입니다. 각 문서는 가능한 한 이 사전의 항목으로 링크됩니다.

## action

[Action](#action)은 오래 걸리는 목표 수행 통신입니다. 예시는 "목표 지점까지 이동", "물체를 집기", "충전 도크로 복귀"입니다. 중간 진행률과 취소가 필요하면 [service](#service)보다 action이 적합합니다.

## agent-tool

[Agent Tool](#agent-tool)은 [LLM Agent](#llm-agent)가 호출할 수 있는 함수입니다. 로봇에서는 `get_robot_state`, `plan_path`, `validate_command`처럼 읽기/검증 도구와 쓰기 도구를 분리해야 합니다.

## ai-agent

[AI Agent](#ai-agent)는 모델이 도구를 호출하며 목표를 수행하는 프로그램 구조입니다. 로봇에서는 agent가 직접 `/cmd_vel`을 publish하지 않고 검증 가능한 계획을 만들어야 안전합니다.

## behavior-tree

[Behavior Tree](#behavior-tree)는 로봇 행동을 조건, 순서, fallback으로 구성하는 트리 구조입니다. [Nav2](#nav2)는 behavior tree를 사용해 주행과 복구 행동을 관리합니다.

## closed-loop

[Closed-loop](#closed-loop)는 모델 출력이 환경을 바꾸고, 바뀐 환경이 다시 다음 입력으로 들어오는 평가입니다. 로봇에서는 open-loop 정확도보다 closed-loop 성공률이 더 중요합니다.

## cpp

[C++](#cpp)는 성능과 지연 시간이 중요한 로봇 노드에서 자주 쓰는 프로그래밍 언어입니다. ROS 2의 대표 C++ 클라이언트 라이브러리는 [rclcpp](#rclcpp)입니다.

## dry-run

[Dry-run](#dry-run)은 실제 모터 명령을 보내지 않고 센서 입력, 모델 추론, 안전 필터, 로그만 검증하는 모드입니다. 실제 로봇 첫 테스트의 필수 단계입니다.

## embedded-optimization

[Embedded Optimization](#embedded-optimization)은 Jetson, NUC, ARM 보드 같은 제한된 장치에서 모델을 빠르고 안정적으로 실행하도록 줄이고 최적화하는 작업입니다.

## executor

[Executor](#executor)는 ROS 2 callback을 어떤 스레드에서 어떤 순서로 처리할지 관리하는 실행기입니다. 지연 시간이 중요한 시스템에서는 executor 설계가 중요합니다.

## gazebo

[Gazebo](#gazebo)는 ROS 2와 잘 연결되는 오픈소스 로봇 시뮬레이터입니다. [ros_gz](#ros-gz) bridge를 통해 Gazebo Transport와 ROS 2 topic을 연결합니다.

## inference

[Inference](#inference)는 학습된 모델을 사용해 새 입력에서 출력을 계산하는 단계입니다. 로봇에서는 inference 시간이 길면 제어 지연이 커집니다.

## isaac-sim

[Isaac Sim](#isaac-sim)은 NVIDIA의 로봇 시뮬레이터입니다. ROS 2 Bridge, RTX 센서, 합성데이터, GPU 기반 로봇 AI 실험에 강합니다.

## langchain

[LangChain](#langchain)은 LLM application과 agent를 구성하는 Python 프레임워크입니다. 로봇에서는 tool 호출, guardrail, 평가, 관측성 설계와 연결됩니다.

## latency

[Latency](#latency)는 입력이 들어온 시점부터 출력이 나올 때까지 걸리는 시간입니다. 로봇에서는 평균뿐 아니라 p90, p99 지연이 중요합니다.

## lifecycle-node

[Lifecycle Node](#lifecycle-node)는 configure, activate, deactivate, cleanup 같은 상태 전이를 가진 ROS 2 노드입니다. 실제 로봇에서는 준비되지 않은 노드가 명령을 내보내지 않도록 제어하는 데 좋습니다.

## llamaindex

[LlamaIndex](#llamaindex)는 문서, 데이터, 검색, workflow, agent를 연결하는 LLM framework입니다. 로봇에서는 매뉴얼/RAG/점검 절차 검색에 유용합니다.

## llm

[LLM](#llm)은 Large Language Model, 즉 대형언어모델입니다. 로봇에서는 자연어 명령 해석, 작업 계획, 코드/스크립트 초안 생성에 쓰일 수 있습니다.

## llm-agent

[LLM Agent](#llm-agent)는 [LLM](#llm)이 도구를 선택하고 호출하면서 목표를 수행하는 구조입니다. 실제 로봇에서는 권한 분리와 안전 검증이 핵심입니다.

## mujoco

[MuJoCo](#mujoco)는 접촉 동역학과 제어 연구에 많이 쓰이는 오픈소스 물리 엔진입니다. 강화학습과 조작 로봇 실험에 적합합니다.

## nav2

[Nav2](#nav2)는 ROS 2의 모바일 로봇 자율주행 스택입니다. localization, costmap, planner, controller, recovery behavior를 포함합니다.

## nlp

[NLP](#nlp)는 Natural Language Processing, 즉 자연어처리입니다. 로봇에서는 명령 해석, 작업 순서 추출, 로그 요약에 사용됩니다.

## node

[Node](#node)는 ROS 2에서 하나의 책임을 가진 실행 단위입니다. 예시는 카메라 노드, 추론 노드, 주행 제어 노드입니다.

## onnx

[ONNX](#onnx)는 여러 딥러닝 프레임워크와 런타임 사이에서 모델을 교환하기 위한 형식입니다. PyTorch에서 학습한 모델을 C++ 추론으로 옮길 때 자주 사용합니다.

## onnx-runtime

[ONNX Runtime](#onnx-runtime)은 ONNX 모델을 CPU/GPU/가속기에서 실행하는 추론 런타임입니다. C++ API를 통해 ROS 2 노드 안에서 모델을 실행할 수 있습니다.

## parameter

[Parameter](#parameter)는 ROS 2 노드의 설정값입니다. 예시는 최대 속도, frame 이름, 모델 경로, 안전 거리입니다.

## pybullet

[PyBullet](#pybullet)은 Bullet Physics SDK의 Python 인터페이스입니다. 빠른 로봇 제어, 강화학습, 물리 실험에 유용합니다.

## python

[Python](#python)은 학습 파이프라인, LLM Agent, 빠른 실험에 적합한 언어입니다. 로봇 제품 코드에서는 C++ 노드와 역할을 나눠 사용합니다.

## pytorch

[PyTorch](#pytorch)는 딥러닝 모델 학습에 널리 쓰이는 프레임워크입니다. 학습은 PyTorch, 배포는 [ONNX Runtime](#onnx-runtime) 또는 [TensorRT](#tensorrt)로 나누는 경우가 많습니다.

## qos

[QoS](#qos)는 ROS 2 통신 품질 설정입니다. reliability, durability, history, depth 등이 있으며 센서 데이터와 제어 명령에서 선택 기준이 다릅니다.

## rag

[RAG](#rag)는 Retrieval-Augmented Generation입니다. LLM이 답변하거나 계획할 때 외부 문서 검색 결과를 함께 사용하게 하는 구조입니다.

## rclcpp

[rclcpp](#rclcpp)는 ROS 2의 C++ 클라이언트 라이브러리입니다. C++로 node, publisher, subscriber, timer, parameter를 만들 때 사용합니다.

## real-robot-test

[Real Robot Test](#real-robot-test)는 실제 로봇 하드웨어에서 수행하는 테스트입니다. 시뮬레이션보다 위험하므로 dry-run, 저속 제한, emergency stop, 로그 수집이 먼저입니다.

## ros-gz

[ros_gz](#ros-gz)는 Gazebo와 ROS 2를 연결하는 패키지 모음입니다. `ros_gz_bridge`는 Gazebo Transport message와 ROS 2 message를 서로 변환합니다.

## ros2

[ROS 2](#ros2)는 로봇 애플리케이션을 만들기 위한 오픈소스 라이브러리와 도구 모음입니다. DDS 기반 통신, lifecycle, QoS, Python/C++ 클라이언트 라이브러리를 제공합니다.

## ros2-control

[ros2_control](#ros2-control)은 ROS 2에서 controller와 hardware interface를 표준화하는 프레임워크입니다. 시뮬레이터와 실제 하드웨어 전환을 구조화하는 데 중요합니다.

## rosbag2

[rosbag2](#rosbag2)는 ROS 2 topic 데이터를 기록하고 재생하는 도구입니다. 실패 재현, 데이터셋 수집, 회귀 테스트에 사용합니다.

## rollback

[Rollback](#rollback)은 새 모델이나 설정이 문제를 만들 때 이전 안정 버전으로 되돌리는 계획입니다. 실제 로봇 배포에서는 모델 파일, config, launch 파일을 함께 버전 관리해야 합니다.

## safety-filter

[Safety Filter](#safety-filter)는 정책 출력이 위험할 때 속도를 제한하거나 정지 명령으로 바꾸는 보호 계층입니다. 학습 모델과 실제 actuator 사이에 둡니다.

## service

[Service](#service)는 요청 1번과 응답 1번으로 끝나는 ROS 2 통신 방식입니다. 예시는 "맵 저장", "모드 변경", "진단 정보 요청"입니다.

## sim-to-real

[Sim-to-Real](#sim-to-real)은 시뮬레이션에서 잘 동작한 모델을 실제 로봇으로 옮기는 과정입니다. 센서 노이즈, 지연, 마찰, 조명 차이가 gap을 만듭니다.

## simulator

[Simulator](#simulator)는 실제 로봇 없이 물리, 센서, 환경을 가상으로 실행하는 도구입니다. Gazebo, Isaac Sim, PyBullet, MuJoCo 등이 있습니다.

## tensor

[Tensor](#tensor)는 숫자를 다차원 배열로 표현한 자료구조입니다. 이미지 batch는 보통 `[batch, channel, height, width]` 형태로 다룹니다.

## tensorrt

[TensorRT](#tensorrt)는 NVIDIA GPU에서 딥러닝 추론을 최적화하는 SDK입니다. ONNX 모델을 engine으로 변환하고 precision, fusion, batching을 활용합니다.

## tf

[TF](#tf)는 ROS에서 좌표계 관계를 시간과 함께 관리하는 시스템입니다. 예시는 `map -> odom -> base_link -> camera_link`입니다.

## topic

[Topic](#topic)은 ROS 2에서 연속 데이터가 흐르는 통신 채널입니다. 예시는 `/scan`, `/camera/image_raw`, `/cmd_vel`, `/odom`입니다.

## vla

[VLA](#vla)는 Vision-Language-Action 모델입니다. 이미지와 언어 명령을 입력으로 받아 로봇 action을 생성하는 모델 계열입니다.
