# 2026-09-20 — micro-ROS 실행 의미, XRCE 예산, 지형 통과성

## 오늘의 핵심 요약

- **기초 실무:** 고정 길이 ROS 2 메시지와 `SensorDataQoS`, `OccupancyGrid`의 row-major/`-1..100` 계약을 익힌다.
- **심화·RT:** rclc Executor의 순차 실행·트리거·LET 직관을 배우고, payload/MTU/조각 수/wire bytes를 계산한다.
- **알고리즘:** 높이 격자의 중앙차분 경사와 3×3 거칠기를 결합해 통과성 비용을 만든다.
- **중요한 경계:** 이 패키지는 실제 MCU/rclc/Micro XRCE-DDS 패킷 측정이 아니라, ROS 2 Jazzy에서 빌드·검증 가능한 교육 모델이다.

## 학습 순서 (Reading Order)

1. 이 README의 시스템 구조와 세 가지 계약을 읽는다.
2. `msg/TerrainPatch.msg`에서 고정 크기 wire 계약을 확인한다.
3. `src/mcu_let_source.cpp`의 `sample → snapshot → publish` 순서를 따라간다.
4. `src/transport_budget_monitor.cpp`에서 `ceil(payload/usable)` 조각화 예산을 손으로 계산한다.
5. `src/traversability_estimator.cpp`에서 경사·거칠기 수식을 코드와 연결한다.
6. `src/pipeline_auditor.cpp`가 생산 코드와 독립적으로 무엇을 다시 계산하는지 본다.
7. `paper_review.md`에서 예산 기반 Executor가 해결하는 시간 격리 문제와 한계를 읽는다.

## 시스템 구조

```mermaid
graph TD
  TIMER[20 Hz 주기 트리거] --> MCU[mcu_let_source<br/>입력 스냅샷 + 고정 배열]
  SENSOR[8x8 높이 센서 모델] --> MCU
  MCU -->|/terrain_patch<br/>SensorDataQoS depth 1| DDS[(ROS 2 DDS)]
  DDS --> BUDGET[transport_budget_monitor<br/>XRCE 조각·wire 예산]
  DDS --> TRAV[traversability_estimator<br/>경사 + 3x3 거칠기]
  BUDGET -->|/transport_stats| AUDIT[pipeline_auditor<br/>독립 재계산]
  TRAV -->|/traversability_fixed| AUDIT
  TRAV -->|/traversability_grid| RVIZ[RViz / Nav 소비자]
  DDS -->|동일 sequence 입력| AUDIT
  AUDIT -->|PASS 또는 FAIL| RESULT[/pipeline/audit]
```

더 큰 화면에서 관계를 탐색하려면 `architecture.html`을 연다. 작성 언어는 한국어이며, 고정 Viewer UI는 영어로 표시된다.

## 1. 기초 실무 — 메시지와 QoS 계약

`TerrainPatch`는 64개 `float32` 고도와 64개 유효 플래그를 고정 배열로 가진다. MCU에서 `std::vector` 크기가 매 주기 달라지는 상황을 피하고, 직렬화 메모리 상한을 설계 시점에 계산하기 위해서다. `/terrain_patch`는 `SensorDataQoS`와 depth 1을 사용한다. 센서 스트림은 오래된 모든 샘플의 완전 전달보다 최신 샘플의 낮은 지연이 더 중요하다는 선택이다.

`OccupancyGrid.data`는 `row * width + col` 순서다. 이 실습에서는 경계를 계산하지 않은 셀은 `-1`, 계산된 통과성 위험은 `0..100`으로 표현한다. 메시지 이름은 OccupancyGrid지만 값은 **점유 확률이 아니라 교육용 위험 비용**이므로 실제 Nav2 연결 전에는 costmap layer 계약으로 변환해야 한다.

## 2. 심화·RT — rclc LET 직관과 전송 예산

rclc Executor는 콜백 등록 순서, 트리거 조건, LET 데이터 의미를 제공한다. LET 모드의 핵심 직관은 주기 시작에 입력을 로컬 복사하고, 미리 정한 순서로 계산하는 것이다. 이 예제는 rclcpp wall timer 안에서 그 순서를 재현할 뿐 실제 rclc Executor를 실행하지 않는다.

교육용 전송 예산은 다음 가정을 명시한다.

```text
CDR payload 추정 = 340 B
XRCE stream MTU  = 128 B
조각당 XRCE 가정 = 16 B
usable payload   = 128 - 16 = 112 B
fragment count   = ceil(340 / 112) = 4
UDP + IPv4       = 28 B / fragment
wire estimate    = 340 + 4 * (16 + 28) = 516 B
budget           = 520 B  → PASS
```

실제 제품에서는 Wireshark/Agent trace로 세션 헤더, submessage, 신뢰 스트림 ACK, 재전송, 링크 계층을 측정해야 한다. 특히 best-effort 스트림은 MTU보다 큰 메시지를 보낼 수 없고, reliable 스트림은 fragmentation과 history 메모리 비용이 생긴다는 점이 설계 선택을 바꾼다.

## 3. 알고리즘 — 높이 격자 통과성

셀 간격을 `r`이라 할 때 중앙차분은 다음과 같다.

```text
g_x = (z[x+1,y] - z[x-1,y]) / (2r)
g_y = (z[x,y+1] - z[x,y-1]) / (2r)
theta = atan(sqrt(g_x^2 + g_y^2))
```

주변 3×3 높이의 표준편차 `sigma`를 거칠기로 두고, 비용을 다음처럼 제한한다.

```text
C = 100 * clamp(0.75 * theta / 25deg + 0.25 * sigma / 0.04m, 0, 1)
```

이 수식은 설명 가능하고 계산량이 고정이지만 로봇 footprint, 토질, 미끄럼, 자세 동역학은 반영하지 않는다.

## 빌드와 실행

```bash
source /opt/ros/jazzy/setup.bash
colcon build --base-paths daily_robotics/2026-09-20 \
  --build-base build/2026-09-20 \
  --install-base install/2026-09-20 \
  --event-handlers console_direct+
source install/2026-09-20/setup.bash
ros2 launch daily_robotics_2026_09_20 daily_demo.launch.py
```

다른 터미널에서 최종 독립 검증 결과를 읽는다.

```bash
ros2 topic echo /pipeline/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
```

`PASS`, `valid=36`, `fragments=4`, `wire_bytes=516`이 포함되어야 한다. 이 결과는 구조·수식 일치 검증이며 MCU 시간 결정성이나 네트워크 worst-case latency를 증명하지 않는다.

## 실무 확장 과제

1. 실제 micro-ROS 보드에서 동일 IDL을 생성하고 rclc `trigger_one(timer)`와 LET 모드로 옮긴다.
2. UDP/serial 각각 Agent trace를 수집해 `340 B` 추정과 조각별 오버헤드를 실측치로 바꾼다.
3. IMU 자세로 중력축을 보정하고, 로봇 footprint 안의 최대 경사·step height를 추가한다.
4. deadline/liveliness 콜백과 sequence gap 카운터를 결합해 통신 열화를 fail-safe 상태로 연결한다.

## 참고 자료

- [micro-ROS Execution Management — rclc Executor, trigger, LET](https://github.com/micro-ROS/micro-ROS.github.io/blob/master/_docs/concepts/client_library/execution_management/index.md)
- [eProsima Micro XRCE-DDS Client — streams, fragmentation, MTU](https://github.com/eProsima/Micro-XRCE-DDS-Docs/blob/master/docs/client.rst)
- [OMG DDS-XRCE 1.0 specification](https://www.omg.org/spec/DDS-XRCE/1.0/)
- [ROS 2 Jazzy QoS concepts](https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html)
- [ROS 2 Jazzy `nav_msgs/OccupancyGrid`](https://docs.ros.org/en/jazzy/p/nav_msgs/msg/OccupancyGrid.html)
- [Staschulat et al., Budget-based real-time Executor for Micro-ROS](https://arxiv.org/abs/2105.05590)
