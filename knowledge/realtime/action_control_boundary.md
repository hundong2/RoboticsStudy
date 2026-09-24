# ROS 2 Action과 실시간 제어 루프의 경계

## 핵심 원칙

Action은 장시간 명령의 Goal/Feedback/Result/Cancel 의미를 잘 표현하지만, Action callback과 DDS publish 자체가 hard real-time API인 것은 아니다. Goal 객체, 문자열, shared pointer, 직렬화, discovery, middleware lock이 제어 deadline 경로에 들어오지 않게 경계를 만든다.

## 권장 데이터 흐름

```text
Action Goal callback (non-RT)
  ├─ range / finite / policy validation
  ├─ dynamic ROSIDL data → fixed-size command/coefficients
  └─ release-store generation/active flag
                    │
                    ▼
Periodic control kernel (RT candidate)
  ├─ acquire-load fixed command
  ├─ bounded read → compute → limit → write
  └─ atomic/fixed-capacity telemetry snapshot
                    │
                    ▼
Feedback timer (non-RT)
  ├─ snapshot → ROS message serialization
  ├─ throttled Action Feedback
  └─ succeed / abort / canceled exactly once
```

## 메모리 가시성과 소유권

- producer는 fixed command를 완전히 쓴 뒤 `release`로 generation/active를 공개한다.
- consumer는 `acquire`로 새 generation/active를 본 뒤 fixed command를 읽는다.
- 실행 중 producer가 같은 slot을 덮어쓰지 않게 단일 Goal, double buffer, 또는 명확한 ownership protocol이 필요하다.
- telemetry는 lock-free가 보장된 scalar atomic, 검증된 SPSC queue, 또는 프레임워크의 realtime buffer를 사용한다.
- C++ seqlock을 plain non-atomic payload에 순진하게 적용하면 data race/undefined behavior가 될 수 있으므로 메모리 모델을 따로 검증한다.

## Goal/Cancel/Result 정책

- Goal validation과 예약은 장치 명령 전에 원자적으로 끝낸다.
- 동시에 하나만 허용하는지, 새 Goal이 선점하는지, queue하는지 문서화한다.
- Cancel을 수락했다는 응답과 물리적 정지가 완료된 시점을 분리한다.
- 실제 로봇의 Cancel은 현재 상태에서 제한을 지키는 stop trajectory 또는 drive-level quick stop으로 연결한다.
- Result 성공은 “계산 종료”가 아니라 goal/path tolerance와 장치 상태가 모두 만족된 경우여야 한다.

## 검증

- RT 후보 loop 안의 heap allocation, unbounded lock, 로그, ROS publish를 정적/동적으로 검사한다.
- 목표 주기, wake-up lateness, 실행시간, deadline miss, page fault를 서로 다른 지표로 기록한다.
- 일반 커널의 worst observed latency를 WCET 또는 hard-RT 보증이라고 부르지 않는다.
- Action Result와 독립된 observer가 raw command/state에서 허용오차를 다시 계산하도록 한다.

## 연결 실습

- `daily_robotics/2026-09-05`: 이동 Action과 atomic scalar handoff 입문
- `daily_robotics/2026-09-25`: 3축 fixed plan, 500 Hz kernel, 50 Hz feedback, 독립 tracking guard
