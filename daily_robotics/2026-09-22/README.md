# 2026-09-22 — ROS 2 Diagnostics, IMM 액추에이터 고장 감지, Fail-safe 상태 머신

## 오늘의 핵심 요약

- **기초 실무:** `sensor_msgs/JointState`의 name/position/velocity 계약과 `diagnostic_msgs/DiagnosticArray`의 OK/WARN/ERROR 상태 보고를 익힌다.
- **심화·RT:** 200 Hz 고정 크기 수치 커널과 20 Hz 가변 길이 진단 경로를 분리하고, Deadline/Liveliness와 로컬 sample-age watchdog을 안전 상태 전이에 연결한다.
- **알고리즘:** 정상 gain=1.0과 출력저하 gain=0.25의 두 Kalman 모델을 IMM으로 섞어 loss-of-effectiveness 확률을 계산한다.
- **검증 목표:** 정상→액추에이터 SAFE_STOP→회복→통신 SAFE_STOP→최종 회복의 순서와 실제 0 명령을 독립 auditor가 확인해야 한다.
- **중요한 경계:** 이 패키지는 단일 관절·1차 동역학의 교육용 fault-injection bench다. 안전 인증, 실제 모터 전원 차단, 완전한 fault isolation, hard real-time을 구현하거나 증명하지 않는다.

## 학습 순서 (Reading Order)

1. 아래 구조도에서 물리 residual 경로와 DDS health 경로가 SAFE_STOP에서 합쳐지는 지점을 찾는다.
2. `src/actuator_simulator.cpp`의 `v_dot=(g*u-v)/tau`와 고장/통신 단절 주입 구간을 읽는다.
3. `src/imm_fault_supervisor.cpp`에서 mixing→predict/correct→mode probability의 수식을 코드와 대조한다.
4. 같은 파일에서 200 Hz hot path와 20 Hz `DiagnosticArray` 경로가 왜 분리되었는지 확인한다.
5. `src/safety_auditor.cpp`가 내부 변수 대신 Topic만으로 상태 순서와 실제 0 출력을 검증하는지 본다.
6. `paper_review.md`에서 논문의 state augmentation이 모델 조합 폭발을 줄이는 방식을 읽는다.
7. `knowledge/ros2/diagnostic_health_contract.md`, `knowledge/sensor_fusion/interacting_multiple_model.md`, `knowledge/realtime/failsafe_supervisor.md`로 복습한다.

## 시스템 구조

```mermaid
graph TD
  SIM[actuator_simulator<br/>200 Hz 명령·1차 plant] -->|/actuator/requested_command<br/>Float64 Reliable| IMM[imm_fault_supervisor]
  SIM -->|/joint_states<br/>JointState + Deadline 15 ms<br/>MANUAL_BY_TOPIC 50 ms| IMM
  IMM -->|정상·복구: rate-limited 전달<br/>BOOTSTRAP·SAFE_STOP: 0| SAFE[/actuator/safe_command]
  IMM -->|20 Hz 상태·확률·QoS 카운터| DIAG[/diagnostics]
  SAFE --> AUDIT[safety_auditor]
  DIAG --> AUDIT
  AUDIT -->|Transient Local PASS / FAIL| RESULT[/actuator/audit]
```

더 큰 화면에서 경로를 탐색하려면 `architecture.html`을 연다. 작성 내용은 한국어이며 고정 Viewer UI는 영어로 표시된다.

## 1. 기초 실무 — JointState와 Diagnostics 계약

`JointState.header.stamp`는 메시지 수신 시각이 아니라 관절 값이 함께 측정된 시각이다. `name[i]`, `position[i]`, `velocity[i]`, `effort[i]`는 같은 관절을 가리키며, 각 배열은 `name`과 같은 크기이거나 비어 있어야 한다. 감독기는 최소 계약으로 `velocity[0]`이 존재하는지 확인하고, 빈 메시지는 `malformed_samples`에 누적한다.

`DiagnosticArray`는 여러 `DiagnosticStatus`를 한 timestamp에 묶는다. 오늘 상태는 다음처럼 해석한다.

| level | 상태 | 의미 |
|---|---|---|
| `OK` | `BOOTSTRAP`, `NORMAL` | 초기화 중이거나 정상 모델이 지배적 |
| `WARN` | `SUSPECT`, `RECOVERING` | 고장 근거가 증가하거나 회복 hysteresis 확인 중 |
| `ERROR` | `SAFE_STOP` | 안전 명령을 실제로 0으로 강제 |

진단의 `values`에는 `degraded_probability`, `sample_age_ms`, `deadline_misses`, `liveliness_losses`, `max_hot_path_us`를 key-value로 넣는다. 문자열·vector 생성은 제어 계산과 분리된 20 Hz 타이머에서만 수행한다.

## 2. 심화·RT — 통신 사건을 안전 상태로 바꾸기

200 Hz 측정의 정상 간격은 5 ms다. 세 주기 여유인 Deadline 15 ms와 장치 heartbeat인 MANUAL_BY_TOPIC Liveliness 50 ms를 함께 둔다. 두 정책은 같은 뜻이 아니다.

- **Deadline:** 기대한 데이터 주기가 지켜졌는가?
- **Liveliness:** Publisher가 lease 안에 생존을 주장했는가?
- **sample age watchdog:** 현재 프로세스가 마지막 유효 샘플을 받은 뒤 25 ms가 넘었는가?

DDS 구현마다 지원·보고 시점이 다를 수 있으므로, QoS 이벤트는 운영 진단 카운터로 보존하고 실제 정지 판단에는 로컬 `steady_clock` sample age도 사용한다. transport fault는 확률 필터를 기다리지 않고 즉시 SAFE_STOP으로 간다.

```text
BOOTSTRAP --정상 근거 300 ms--> NORMAL
NORMAL --p(degraded) >= 0.60--> SUSPECT
NORMAL/SUSPECT --p(degraded) >= 0.90, 100 ms--> SAFE_STOP(ACTUATOR)
ANY --sample age > 25 ms--> SAFE_STOP(TRANSPORT)
SAFE_STOP --정상 근거 300 ms--> RECOVERING --300 ms--> NORMAL
```

IMM 계산은 상태 1개×모델 2개와 `std::array`만 사용하므로 입력 크기에 따라 계산량이 증가하지 않는다. 그러나 전체 Timer 콜백에는 DDS publish, 일반 Linux scheduler, rclcpp executor가 포함된다. 측정된 `max_hot_path_us`는 이 한 시나리오의 관측값일 뿐 WCET나 hard RT 보장이 아니다. 제품에서는 PREEMPT_RT, priority/affinity, 메모리 page fault, allocator, RMW 지원표를 별도로 검증해야 한다.

## 3. 알고리즘 — 두 모델 IMM의 수학적 직관

단일 관절 속도 모델은 다음과 같다.

```text
v_j(k+1) = a v_j(k) + (1-a) g_j u(k) + w(k)
a = exp(-dt/tau),  g_healthy=1.0,  g_degraded=0.25
z(k) = v(k) + n(k)
```

각 모델은 서로 다른 gain으로 다음 속도를 예측한다. IMM 한 스텝은 네 부분이다.

1. **Interaction:** 이전 모드 전이확률 `p_ij`와 확률 `mu_i`로 각 필터의 초기 상태·분산을 섞는다.
2. **Kalman filtering:** 각 gain 모델이 `predict→innovation→correct`를 독립 실행한다.
3. **Likelihood:** residual `r_j=z-x_j^-`를 `N(r_j;0,S_j)`로 평가한다.
4. **Mode update:** `mu_j = likelihood_j*c_j / sum(likelihood_l*c_l)`로 정규화한다.

한 개 residual threshold보다 유리한 점은 모터 관성·측정 잡음을 상태 공분산으로 다루고, “정상인가/고장인가”를 확률과 지속 시간으로 표현한다는 것이다. 반대로 모델에 없는 마찰, 부하 토크, 전압 저하도 gain 고장처럼 보일 수 있다. 그러므로 IMM mode는 **원인 증명**이 아니라 현재 모델 집합 중 가장 그럴듯한 설명이다.

## 고장 주입 타임라인과 독립 검증

```text
0.0–2.0 s   gain=1.0, JointState 정상
2.0–4.5 s   gain=0.25, 데이터는 계속 도착
4.5–6.5 s   gain=1.0, 회복 hysteresis 확인
6.5–7.1 s   JointState와 liveliness 모두 중단
7.1 s 이후  데이터 복귀와 최종 회복 확인
```

`safety_auditor`는 감독기의 C++ 멤버를 읽지 않는다. `/diagnostics`의 상태·근거와 `/actuator/safe_command`의 실제 0을 별도로 구독한다. 다음 다섯 조건과 5 ms callback의 5,000 us 미만 관측값을 모두 만족해야 `PASS`를 Transient Local로 게시한다.

```text
initial_normal → actuator_stop → actuator_recovery
               → transport_stop → final_recovery
```

## 빌드와 실행

```bash
source /opt/ros/jazzy/setup.bash
colcon build --base-paths daily_robotics/2026-09-22 \
  --build-base build/2026-09-22 \
  --install-base install/2026-09-22 \
  --event-handlers console_direct+
source install/2026-09-22/setup.bash
ros2 launch daily_robotics_2026_09_22 daily_demo.launch.py
```

별도 터미널에서 최종 결과를 읽는다.

```bash
ros2 topic echo /actuator/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
```

`PASS initial_normal=1 actuator_stop=1 actuator_recovery=1 transport_stop=1 final_recovery=1`가 포함되어야 한다. 이 PASS는 합성 1차 plant와 지정된 시간표의 회귀 검증이지 실제 장비 안전 인증이 아니다.

## 실무 확장 과제

1. 좌/우 바퀴를 각각 정상·저하·stuck·bias 모델로 늘리고 모델 수 증가량과 진단 지연을 측정한다.
2. 전류·속도·토크를 상태 증강해 “출력 저하”와 “센서 bias”를 분리한다.
3. SAFE_STOP을 단순 Topic 0이 아니라 독립 safety MCU의 STO(Safe Torque Off) 회로와 연결한다.
4. 장애 구간 rosbag2를 deterministic replay해 false positive/negative, detection delay, recovery delay를 통계화한다.
5. `ros2_tracing`으로 DDS take부터 안전 출력 publish까지 end-to-end latency와 tail percentile을 측정한다.

## 참고 자료

- [ROS 2 `JointState` 원본 메시지](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/JointState.msg)
- [ROS 2 `DiagnosticStatus` 원본 메시지](https://github.com/ros2/common_interfaces/blob/rolling/diagnostic_msgs/msg/DiagnosticStatus.msg)
- [ROS 2 `DiagnosticArray` 원본 메시지](https://github.com/ros2/common_interfaces/blob/rolling/diagnostic_msgs/msg/DiagnosticArray.msg)
- [ROS 2 Jazzy `SubscriptionOptionsBase`와 QoS 이벤트 콜백](https://docs.ros.org/en/jazzy/p/rclcpp/generated/structrclcpp_1_1SubscriptionOptionsBase.html)
- [ROS 2 Jazzy RMW liveliness changed 상태](https://docs.ros.org/en/ros2_packages/jazzy/api/rmw/generated/structrmw__liveliness__changed__status__s.html)
- [Zhong et al., Actuator and Sensor Fault Detection and Diagnosis for Unmanned Quadrotor Helicopters](https://doi.org/10.1016/j.ifacol.2018.09.708)
