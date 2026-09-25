# 다중 접촉 힘 분배 QP

## 표준 형태

원하는 로봇 전체 wrench `w_des`를 여러 접촉력 `f`로 만들 때 가장 작은 문제는 다음과 같다.

```text
min_f  1/2 ||A f - w_des||²_W + lambda/2 ||f - f_ref||²
subject to  C f <= d
```

- `A`: 각 접촉력의 힘과 moment arm을 전체 wrench로 합치는 행렬
- `W`: force/moment task의 상대 우선순위
- `f_ref`: 이전 해, 균등 하중, 또는 선호하는 하중 분배
- `C f <= d`: friction, unilateral contact, normal-force, CoP, torque 한계

## 왜 해가 여러 개인가

두 발의 4개 평면 힘 성분으로 3개 net wrench 성분을 만들면 일반적으로 null-space가 남는다. 같은 전체 wrench라도 내부적으로 서로 미는 힘이나 좌우 하중 분배가 다를 수 있다. `lambda ||f-f_ref||²` 같은 항은 이 자유도를 일관되게 고르고 해의 급변을 줄인다.

## 마찰 제약

평면 Coulomb 모델은 다음과 같다.

```text
Fz >= 0
|Fx| <= mu Fz
```

3D 원뿔은 `sqrt(Fx²+Fy²) <= mu Fz`다. 선형 QP가 필요하면 여러 평면으로 friction pyramid를 만들 수 있지만 원뿔에 대해 보수적이며 면 수와 계산량 사이 trade-off가 생긴다.

## solver 결과 소비 계약

다음 값을 매 tick 함께 내보내야 한다.

- primal constraint violation
- task residual `||Af-w_des||`
- optimality 또는 projected-gradient/KKT residual
- iteration count와 time budget 상태
- solver status: optimal, feasible approximate, infeasible, numerical failure
- warm-start source와 contact set revision

`success=true` 하나만 보내면 degraded solution과 stale solution을 구분할 수 없다.

## 접촉 전환

발이 떨어지는 순간 해당 접촉 변수와 제약을 제거하거나 force를 0으로 고정해야 한다. contact set이 바뀌면 이전 warm start가 새 feasible set 밖일 수 있으므로 projection/초기화 정책이 필요하다. 접촉 추정의 hysteresis와 timestamp도 QP 계약의 일부다.

## 관련 실습

- `daily_robotics/2026-09-26`: 두 발 평면 QP와 잘린 마찰 원뿔 projection
- `daily_robotics/2026-09-09`: 단일 관절 제약 MPC
