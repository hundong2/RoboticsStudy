# Bounded Visual Feature Tracking

## 평균 FPS보다 먼저 고정할 것

Feature frontend의 계산량은 보통 다음 곱으로 커진다.

\[
C\approx N_{candidate}C_{score}+N_{track}L_{pyramid}N_{iteration}A_{patch}
\]

RANSAC까지 포함하면 hypothesis 수도 추가된다. 실시간 경계를 만들려면 image size만 줄이는 것이 아니라 후보 수, 선택 수, pyramid level, iteration, patch/search 크기, RANSAC hypothesis를 함께 제한한다.

## 권장 실행 경계

1. DDS callback에서는 timestamp/shape/encoding을 검증하고 bounded queue로 ownership을 넘긴다.
2. numerical worker는 wakeup당 처리 frame 수를 제한한다.
3. queue full policy(drop oldest/newest, backpressure)를 명시하고 drop counter를 공개한다.
4. feature detector와 tracker는 fixed-capacity scratch storage를 재사용한다.
5. pose estimator/global optimizer와 tracking deadline을 분리한다.
6. quality와 timing을 독립 auditor가 원본 timestamp 기준으로 확인한다.

## 품질 telemetry

- candidate/selected/tracked/inlier count
- minimum eigenvalue 또는 descriptor response
- spatial grid occupancy/entropy
- photometric residual과 forward-backward error
- parallax, reprojection residual, model conditioning
- queue/DDS drop, stale frame, processing age
- latest/max/tail latency와 budget miss

Feature count만 많으면 된다는 판단은 위험하다. 동적 물체에 feature가 몰리거나 한 edge/plane에 집중되면 geometry는 여전히 퇴화한다.

## SPSC handoff의 경계

Acquire/release SPSC는 producer와 consumer가 각각 정확히 하나일 때 간단하고 유용하다. 다음 경우에는 그대로 확장할 수 없다.

- reentrant callback 또는 여러 image publisher가 같은 queue에 push
- consumer가 여러 worker로 분산
- message payload lifetime이 publisher middleware loan과 충돌
- producer 평균 rate가 consumer 처리율을 지속적으로 초과

고정 queue는 overload를 없애지 않고 **overload 동작을 유한하고 관측 가능하게** 만든다.

## WCET에 대한 정직한 표현

고정 feature 수와 iteration은 algorithmic work bound다. 다음까지 포함한 WCET 증명은 아니다.

- DDS serialization/deserialization과 allocator
- image payload copy와 cache miss
- page fault, interrupt, scheduler preemption
- CPU frequency/thermal throttling
- library 내부 vectorization/parallelism

따라서 실행시간 telemetry는 hardware/build/workload와 함께 기록하고, 실제 RT 요구에는 PREEMPT_RT, affinity, memory locking, tracing, stress 조건 검증을 추가한다.
