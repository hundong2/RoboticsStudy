# Bounded QP Solver와 진단 계약

## “빠른 평균”과 “bounded”의 차이

평균 solve time이 50 us여도 일부 입력에서 10 ms가 걸리면 1 kHz 제어기에는 쓸 수 없다. RT 경로에는 입력 크기, 반복 수, 메모리, 분기, 실패 처리의 상한이 필요하다.

## 고정 반복 solver 패턴

```text
for k in 0..K-1:
    gradient = Hx + g
    x = project(x - alpha * gradient)
```

고정 `K`는 계산량을 예측하기 쉽게 하지만 최적성 보장은 입력마다 달라진다. 따라서 출력에 residual과 feasibility를 포함하고, 소비자가 허용 기준 밖이면 이전 안전 명령/정지/단순 fallback을 선택해야 한다.

## RT와 ROS 경계

권장 분리는 다음과 같다.

```text
ROS callback --fixed queue--> numerical kernel --fixed queue--> ROS publisher
```

- callback에서 ROS 메시지를 고정 크기 POD로 변환한다.
- kernel은 `std::array`와 사전 계산된 행렬을 사용한다.
- kernel에서 logging, parameter lookup, DDS publish, 예외, heap allocation을 피한다.
- queue full 정책(drop newest/drop oldest/fault)을 명시하고 count를 공개한다.
- `sleep_until` 또는 hardware timer로 절대 주기 경계를 사용한다.

## 필수 진단

- `iterations`: configured bound와 실제 실행 수
- `solve_time`: 관측 latency; WCET와 구분해 표기
- `constraint_violation`: primal feasibility
- `task_residual`: 명령 달성 오차
- `optimality_residual`: projected gradient 또는 KKT residual
- input/output sequence와 timestamp
- queue drop/overrun count
- numeric finite check
- 플랫폼 atomic lock-free 확인 결과

## 검증 단계

1. 단위 테스트로 projection과 행렬 부호를 확인한다.
2. infeasible/경계/NaN 입력을 포함한 stress test를 한다.
3. 독립 구현이 `Af`, 제약, actuator torque를 재계산한다.
4. tracing으로 callback-to-actuation latency 분포를 측정한다.
5. 목표 하드웨어와 RT kernel에서 page fault, scheduler, IRQ, cache 간섭을 포함해 분석한다.
6. failure injection으로 queue full, stale input, solver non-convergence의 fallback을 검증한다.

관측 최대값은 시험 범위 안의 증거일 뿐 WCET 증명이 아니다.

## 관련 실습

- `daily_robotics/2026-09-26`: 64회 projected-gradient, SPSC 경계, 독립 감사
- `knowledge/realtime/callback_budget_measurement.md`: callback 지연과 실행시간 계측
- `knowledge/realtime/ros2_tracing_lttng.md`: end-to-end tracing
