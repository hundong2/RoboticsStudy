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
