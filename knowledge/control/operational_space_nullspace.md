# Operational-Space Control과 동적 일관 Null Space

## 왜 joint PD만으로 부족한가

작업 목표가 손끝의 위치/힘인데 관절별 오차만 제어하면 자세에 따라 손끝에서 느끼는 inertia와 gain이 크게 달라진다. Operational-space control은 task coordinate `x`에서 원하는 응답을 설계한다.

```text
x_dot  = J q_dot
x_ddot = J q_ddot + J_dot q_dot
Lambda = (J M^-1 J^T)^-1
```

`Lambda`는 task-space apparent inertia다. 같은 Cartesian acceleration이라도 posture에 따라 필요한 force가 달라지는 효과를 담는다.

## 동적 일관 generalized inverse

```text
J_bar = M^-1 J^T Lambda
N     = I - J_bar J
```

저우선순위 joint torque `tau_0`를 `N^T tau_0`로 투영하면 이상적인 full-rank 조건에서

```text
J M^-1 N^T tau_0 = 0
```

이다. 즉 secondary torque가 primary task acceleration에 결합되지 않는다. 단순 kinematic projector `I-J^+J`는 속도 수준에는 맞아도 torque/dynamics metric에서 같은 decoupling을 보장하지 않는다.

## 기본 토크 구조

```text
a*  = x_ddot_d + Kd e_dot + Kp e - J_dot q_dot
tau = h(q,q_dot) + J^T Lambda a* + N^T tau_0
```

`tau_0`에는 posture, manipulability, joint-limit avoidance 등을 넣을 수 있다. 그러나 여러 secondary objective와 부등식 제한이 생기면 projector chain만으로는 saturation/feasibility를 다루기 어렵다. hierarchical QP, SNS, constrained inverse dynamics를 고려한다.

## 특이점

`J M^-1 J^T`의 작은 singular value는 특정 task 방향의 제어 authority가 사라짐을 뜻한다. 단순 damping

```text
Lambda_lambda = (J M^-1 J^T + lambda^2 I)^-1
```

은 토크 폭주를 줄이지만 정확한 decoupling과 추종 성능을 일부 포기한다. 그래서 `lambda` 활성 여부, condition, task error, torque saturation을 함께 관측해야 한다.

## 관련 학습

- 구현: `daily_robotics/2026-09-28`
- 기초 operational-space: Khatib (1987, 1993)
- multi-contact/whole-body 확장: Khatib et al. (IJRR 2022), DOI `10.1177/02783649221120029`
