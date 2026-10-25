# Bounded Geometric RANSAC

## 왜 별도 실행 계약이 필요한가

전통 RANSAC은 원하는 성공확률이 될 때까지 iteration을 늘리거나 현재 inlier 비율로 조기 종료한다. perception accuracy에는 좋지만 입력 수와 iteration이 무제한이면 callback worst-case를 설명하기 어렵다.

## 명시해야 할 상한

- 입력 correspondence 최대 수
- hypothesis 최대 수
- minimal solver 한 번의 고정 작업량
- hypothesis당 residual 평가 수
- local optimization/refinement 횟수
- queue capacity와 wakeup당 drain 수
- 실패·퇴화 시 pose hold/propagation 정책

`M`개 입력과 `H`개 hypothesis를 고정하면 기본 score 작업은 `O(HM)`이며 최대 residual 평가는 `H*M`으로 바로 계산할 수 있다.

## 확률과 latency trade-off

inlier 비율 `w`, minimal sample 크기 `s`, hypothesis 수 `N`에서 한 번 이상 all-inlier sample을 얻을 확률은

\[
p=1-(1-w^s)^N
\]

이다. `N` cap은 deadline을 만들지만 miss probability를 0으로 만들지 않는다. 기록된 실제 `w` 분포로 cap을 validation하고, 실패 시 estimator가 이전 pose를 유지하거나 IMU/wheel propagation으로 degrade해야 한다.

## RT 구현 패턴

- DDS callback은 shared pointer만 fixed SPSC에 전달한다.
- worker는 wakeup당 고정 개수만 처리한다.
- `std::array`와 compile-time cap으로 hot path heap 성장을 없앤다.
- deterministic pair schedule은 재현성이 좋지만 adversarial ordering에 취약하다.
- inlier count 외에 residual, spatial condition, parallax, hypothesis 수, queue drop, kernel time을 publish한다.
- 측정된 max time은 WCET가 아니다. PREEMPT_RT, affinity, page fault, allocator, RMW까지 별도 검증한다.

대표 실습: `daily_robotics/2026-10-26`.
