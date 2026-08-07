# C++ ML Policy Node 예제

이 예제는 [LaserScan](../../glossary/README.md#topic)을 입력으로 받아 간단한 정책 추론을 수행하고, 원시 속도 명령을 `/policy/cmd_vel_raw`로 publish하는 [C++](../../glossary/README.md#cpp) [ROS 2](../../glossary/README.md#ros2) 패키지 구조입니다.

## 파일

- [package.xml](package.xml): ROS 2 패키지 메타데이터
- [CMakeLists.txt](CMakeLists.txt): C++ 빌드 설정
- [src/ml_policy_node.cpp](src/ml_policy_node.cpp): ML policy 추론 노드
- [src/safety_filter.cpp](src/safety_filter.cpp): 안전 필터 노드
- [config/policy_params.yaml](config/policy_params.yaml): parameter 예시
- [launch/sim_policy_pipeline.launch.py](launch/sim_policy_pipeline.launch.py): 시뮬레이션용 launch 예시

## 빌드 예시

```bash
cd ~/ros2_ws/src
cp -r <repo>/ROS/ai_robotics_cpp_pipeline/examples/cpp_ml_policy_node .
cd ~/ros2_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select cpp_ml_policy_node
source install/setup.bash
```

## 실행 예시

```bash
ros2 launch cpp_ml_policy_node sim_policy_pipeline.launch.py
```

## 테스트 아이디어

1. Gazebo 또는 rosbag2에서 `/scan`을 publish합니다.
2. `ml_policy_node`가 `/policy/cmd_vel_raw`를 내는지 확인합니다.
3. `safety_filter`가 `/cmd_vel`을 내는지 확인합니다.
4. 장애물이 가까운 bag을 재생해 정지 명령이 나오는지 확인합니다.

## 실제 ONNX Runtime 연결

이 예제의 `run_policy` 함수는 학습 문서용 fallback 로직입니다. 실제 프로젝트에서는 같은 위치에 [ONNX Runtime](../../glossary/README.md#onnx-runtime) C++ API를 연결합니다. 초보자가 먼저 ROS 통신과 안전 구조를 이해한 뒤 ONNX Runtime을 붙이는 순서를 권장합니다.
