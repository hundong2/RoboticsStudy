# Dynamic Window Approach (DWA)

## 핵심 모델

DWA는 짧은 구간에 일정한 `(v,w)`가 직선/원호 궤적을 만든다는 점을 이용해 velocity space에서 지역 명령을 고른다.

```text
V_search = V_limits ∩ V_dynamic ∩ V_admissible
```

- 물리적 속도 한계 안
- 현재 속도에서 한 제어주기 동안 도달 가능
- 원호상의 장애물 전에 제동 가능

## 최소 구현 단계

1. 현재 `v,w`, 가속도 한계, 제어주기로 dynamic window 계산
2. 고정 개수의 `(v,w)` 샘플 생성
3. 각 후보를 unicycle model로 일정 horizon rollout
4. footprint 충돌과 braking admissibility 검사
5. heading, clearance, velocity 등 정규화 score로 선택
6. 독립 command guard에서 최종 속도·가속도·stale timeout 제한

## 계산량

후보 수 `N_v*N_w`, rollout step `N_t`, 장애물 점 `N_o`인 단순 구현은 대략 `O(N_v N_w N_t N_o)`다. 각 상한을 고정하면 WCET 분석의 출발점이 되지만 DDS/executor/OS jitter는 별도 측정해야 한다.

## 제품화 보강

- polygon footprint swept collision
- 선/각속도 양쪽의 실제 제동곡선
- perception→actuation latency margin
- 동적 장애물 tracking과 TTC
- global path alignment, goal critic, oscillation suppression
- stale scan/odometry fail-safe
- 후보별 탈락 이유와 최소 clearance telemetry

## 원 논문

- Dieter Fox, Wolfram Burgard, Sebastian Thrun, “The Dynamic Window Approach to Collision Avoidance,” *IEEE Robotics & Automation Magazine*, 1997. [DOI 10.1109/100.580977](https://doi.org/10.1109/100.580977)
