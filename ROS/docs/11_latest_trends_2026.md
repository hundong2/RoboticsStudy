# 11. 2026 ROS 최신 트렌드

기준일: 2026-07-12

## 배포판 흐름

| 배포판 | 성격 | 학습/실무 판단 |
|---|---|---|
| Lyrical Luth | 2026년 LTS | 최신 LTS. Ubuntu 26.04와 장기 운영 학습 기준 |
| Jazzy Jalisco | 2024년 LTS | Ubuntu 24.04 기반 안정 학습과 기존 생태계 호환에 좋음 |
| Kilted Kaiju | 2025년 standard release | Zenoh Tier 1 등 전환기 기능 학습에 유용 |
| Rolling Ridley | rolling release | 최신 API 확인용. 제품 기준으로는 고정 배포판 권장 |

## 주목할 흐름

### Physical AI

Open Robotics의 2026 전략은 Physical AI 지원 강화를 주요 방향으로 제시합니다. 이는 ROS가 전통적인 로봇 미들웨어를 넘어 데이터 수집, 학습, 추론, 시뮬레이션, 배포 파이프라인과 더 강하게 연결되어야 함을 뜻합니다.

### Zenoh와 다양한 RMW

Kilted에서 Zenoh가 Tier 1 RMW로 다뤄지면서 DDS 외 통신 선택지가 더 중요해졌습니다. 멀티 로봇, 네트워크 품질이 불안정한 환경, edge-cloud 연결에서는 RMW 선택이 아키텍처 결정이 됩니다.

### Zero-copy와 GPU 데이터 경로

Lyrical은 GPU 데이터 이동 비용을 줄이는 방향의 기능을 포함합니다. 이미지, point cloud, 딥러닝 추론처럼 큰 데이터를 다루는 로봇에서는 copy 횟수가 latency와 전력에 직접 영향을 줍니다.

### rclpy asyncio

Python 노드도 비동기 I/O, service call, timer, 외부 API 연동을 더 자연스럽게 다루는 방향으로 발전하고 있습니다. 로봇 + 웹 API + 데이터 파이프라인을 연결할 때 중요합니다.

### rosbag2 관측성

rosbag2는 단순 기록 도구를 넘어 원격 제어, message loss 관측, 회귀 테스트 자료로 중요도가 커지고 있습니다. 전문가 수준에서는 bag을 테스트 fixture로 다룰 수 있어야 합니다.

### 시뮬레이션 기반 검증

Gazebo, Isaac Sim, synthetic data, CI simulation smoke test가 제품화 흐름에 들어오고 있습니다. 실제 하드웨어 테스트만으로는 회귀와 안전 검증을 충분히 반복하기 어렵기 때문입니다.

### Nav2, MoveIt2, ros2_control의 제품화

모바일 로봇과 조작 로봇 모두 ROS 2 중심의 표준 스택이 성숙하고 있습니다. 직접 알고리즘을 모두 만들기보다 표준 스택의 설정, 확장, 디버깅 능력이 더 중요해졌습니다.

## 학습자가 해야 할 선택

- 처음 배우면 Lyrical 또는 Jazzy 중 OS에 맞는 LTS를 고릅니다.
- 회사/프로젝트가 이미 특정 배포판을 쓰면 그 배포판을 우선합니다.
- 최신 기능 실험은 Rolling 또는 별도 컨테이너에서 합니다.
- 제품 코드는 배포판, RMW, Gazebo 버전, Docker image digest를 고정합니다.

## 공식 근거

- ROS 2 Lyrical 릴리스 공지: <https://discourse.openrobotics.org/t/ros-2-lyrical-luth-released/55021>
- ROS 2 릴리스 태그: <https://github.com/ros2/ros2/releases>
- ROS 2 REP-2000: <https://reps.openrobotics.org/rep-2000/>
- OSRF 2026 기술 전략: <https://osralliance.org/open-robotics-technology-strategy-for-2026/>
