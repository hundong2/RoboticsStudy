# Bounded Multi-rate Factor Scheduling

## 문제

고속 sensor callback에서 graph update를 직접 수행하면 callback execution time과 queueing delay가 data-dependent해진다. 서로 다른 topic은 같은 timestamp라도 도착 순서가 보장되지 않는다.

## 권장 경계

```text
sensor callback -> fixed-capacity SPSC -> timestamp staging -> bounded worker -> numerical kernel
```

- callback은 validate/copy/enqueue만 한다.
- worker 한 번의 drain 수와 factor 수를 제한한다.
- future timestamp sample을 현재 keyframe 구간에 섞지 않는다.
- queue/staging overflow와 stale drop을 반드시 외부 metric으로 낸다.

## Memory-order 의미

단일 producer/single consumer ring에서 producer는 payload를 쓴 뒤 `head.store(..., release)`로 공개한다. consumer는 `head.load(..., acquire)` 뒤 payload를 읽는다. 이 패턴은 MPSC나 reentrant producer에는 그대로 적용할 수 없다.

## 상한을 주장하려면 함께 기록할 값

- queue capacity와 high-water mark
- wakeup당 최대 drain 수
- keyframe당 preintegrated sample 수
- active states/variables/factors
- solver iteration과 pivot/failure
- drop/stale/out-of-order count
- callback/kernel latency p50/p95/p99/max

## 흔한 오해

- `MultiThreadedExecutor`만 사용한다고 우선순위 격리가 되지 않는다.
- lock-free queue가 message allocation과 DDS copy를 없애지 않는다.
- bounded memory가 bounded execution time을 자동 보장하지 않는다.
- 관측된 최대 시간은 WCET가 아니다.

## 관련 실습

- `daily_robotics/2026-10-24`: 64-slot IMU SPSC, 32-slot timestamp staging, wakeup당 IMU 8/visual 1, 6-state graph
