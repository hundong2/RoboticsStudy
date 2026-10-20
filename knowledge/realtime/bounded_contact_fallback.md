# Bounded contact fallback supervisor

## 목적

고주기 접촉 감독기는 입력 크기에 따라 계산량이 늘지 않아야 하고, estimator와 failure mode를 공유하지 않아야 한다. 고정 필드 메시지, scalar 상태, 상수 개수 비교/곱셈으로 hot path를 구성하면 실행시간 상한을 분석하기 쉽다.

## 상태 기계

```text
DISARMED --50 good--> ACTIVE --3 bad--> FALLBACK
    ^                    ^                |
    | contact lost       | 400 ms +       |
    +--------------------+ 80 good -------+
```

`bad`는 estimator mode 하나가 아니라 slip speed, friction margin, sample age를 독립적으로 OR한다. 출력은 어떤 상태에서도 보수 한계를 넘지 않도록 마지막 actuator boundary에서 다시 clamp한다.

## 검증

- transition coverage: AIR/STABLE/SLIP/FALLBACK/RECOVERY를 모두 관찰
- trip latency: 첫 실제 slip과 첫 fallback 사이의 timestamp 차이
- invariant: `|allowed| <= conservative_limit`; FALLBACK이면 `allowed=0`
- timing: callback sample max와 sample age를 기록하되 WCET라고 표현하지 않음
- late subscriber: PASS 결과는 transient-local로 보존

2026-09-30 실습은 `daily_robotics/2026-09-30/src/contact_safety_auditor.cpp`에서 이 조건을 독립 검사한다.

## 센서 callback과 estimator kernel 사이의 SPSC 경계 (2026-10-21)

ROS sensor message는 string이나 동적 배열을 포함할 수 있다. RT estimator에는 이를 그대로 쌓지 말고, non-RT 입력 callback에서 frame·stamp·크기·finite 여부를 검증한 뒤 필요한 scalar만 고정 구조체로 추출한다.

single producer/single consumer라면 고정 ring과 acquire/release index로 mutex 없는 handoff를 만들 수 있다. 중요한 계약은 다음과 같다.

- 큐가 가득 차면 block/resize하지 않고 drop을 계수한다.
- estimator callback은 한 번에 처리할 최대 표본 수를 고정한다.
- sample `header.stamp`로 `dt`를 계산하고 non-monotonic/과도한 gap을 거부한다.
- `max_batch`, `drop`, `invalid`, 관측 실행시간을 별도 stats로 발행한다.
- estimator와 다른 구현 경로의 auditor가 timestamp pairing과 상한을 재검산한다.

lock-free queue는 전체 ROS 경로의 hard-RT를 뜻하지 않는다. DDS take/deserialize, executor wake-up, publish, OS scheduling과 page fault가 남으므로 trace와 target hardware 부하 시험이 별도로 필요하다.
