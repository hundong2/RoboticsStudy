# Delayed Factor의 실시간 예산과 실패 정책

## 왜 별도 예산이 필요한가

지연 센서나 loop closure는 과거 state를 수정한다. 정확도만 생각하면 가능한 모든 과거를 보존하고 다시 풀고 싶지만, embedded/online 시스템에서는 메모리와 재계산량이 입력 지연에 따라 무한히 커질 수 있다. 따라서 “얼마나 늦은 정보를 받을 것인가”는 estimator 설정이 아니라 시스템 자원 계약이다.

## 설계할 상한

- lag duration 또는 최대 state 수
- 한 cycle에 ingest할 factor 수
- pending queue capacity
- 한 update의 iteration/relinearization/affected-clique 수
- 최대 matrix dimension 또는 nonzero 수
- deadline을 넘겼을 때 publish할 마지막 안전 상태의 age

## 실패 정책

| 상황 | 권장 동작 |
|---|---|
| factor가 창 안에 있음 | 원래 timestamp state에 삽입 |
| factor가 창보다 오래됨 | drop 계수 증가 또는 offline queue로 이동 |
| pending queue full | newest/oldest/priority drop 정책을 명시 |
| solver rank 부족/NaN | 현재 update 폐기, last-known-good 유지, fault 진단 |
| solve deadline 초과 | stale 결과를 숨기지 말고 age/deadline miss 발행 |
| 반복 outlier | robust gate 및 sensor degradation 상태 전환 |

## Timestamp와 queue의 함정

callback arrival order는 measurement-time order가 아니다. DDS topic이 다르면 전역 순서도 기대할 수 없다. queue에는 payload뿐 아니라 measurement stamp, arrival steady time, sequence/source id를 보존해야 한다. ROS time jump가 가능한 재생 환경에서는 duration 계산에 steady clock과 ROS clock을 목적별로 분리한다.

## 구현 패턴

1. non-RT callback이 메시지 계약을 검사한다.
2. bounded queue 또는 preallocated slot에 factor를 복사한다.
3. numerical worker는 한 wake-up에 정해진 수만 처리한다.
4. 결과와 진단을 snapshot으로 publish한다.
5. 독립 supervisor가 freshness, residual, drop, solve failure를 함께 본다.

## 검증 체크리스트

- 가장 오래 허용한 지연과 한 tick 더 오래된 지연을 모두 시험한다.
- burst로 queue capacity 경계를 넘겨 drop 정책을 확인한다.
- out-of-order factor가 잘못된 최신 state에 붙지 않는지 exact-stamp로 검증한다.
- 평균뿐 아니라 max, percentile, deadline miss count를 기록한다.
- 실행시간 관측을 WCET 또는 hard-RT 증명으로 표현하지 않는다.

## 관련 일일 실습

- `daily_robotics/2026-10-20`: 8-state lag, 8 pending factors, 3 fixed iterations, too-old drop 진단
