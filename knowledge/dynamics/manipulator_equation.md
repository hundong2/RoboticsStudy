# Manipulator Equation: `M(q)q_ddot + h(q,q_dot) = tau`

## 대표 형태

고정베이스 rigid manipulator의 관절 동역학은 보통 다음처럼 쓴다.

```text
M(q) q_ddot + C(q,q_dot) q_dot + g(q) + f(q_dot) = tau + tau_ext
```

- `M(q)`: symmetric positive-definite inertia matrix
- `C q_dot`: Coriolis/centrifugal 항
- `g(q)`: gravity generalized force
- `f(q_dot)`: 마찰/감쇠
- `tau_ext`: 접촉 등 외력이 관절로 들어온 값

## 에너지에서 `M(q)` 만들기

각 링크 질량중심 선속도를 `v_k=J_v,k q_dot`, 각속도를 `omega_k=J_w,k q_dot`라 하면

```text
T = 1/2 sum_k (m_k v_k^T v_k + omega_k^T I_k omega_k)
  = 1/2 q_dot^T M(q) q_dot
```

따라서

```text
M(q) = sum_k (m_k J_v,k^T J_v,k + J_w,k^T I_k J_w,k)
```

가 된다. `daily_robotics/2026-09-28`은 3R 평면 링크의 질량중심 Jacobian을 고정 반복으로 계산해 이 식을 그대로 구현한다.

## 중력항 부호를 혼동하지 않는 법

좌표와 potential energy `V(q)`를 먼저 정의하고 `g(q)=dV/dq`를 계산한다. 그 뒤 equation을 `M q_ddot + g = tau`로 고정하면, 정지 자세를 유지하는 feedforward torque는 `tau=g`다. 부호를 기억으로 맞추기보다 작은 자세에서 potential과 torque 방향을 단위 테스트하는 편이 안전하다.

## 실무 체크리스트

- `M`이 모든 정상 자세에서 대칭이고 positive-definite인가?
- controller와 plant/estimator가 같은 joint order와 좌표 부호를 쓰는가?
- payload 변경이 `M`, `g`에 반영되는가?
- friction/dead-zone/backlash를 ideal rigid model과 혼동하지 않는가?
- inverse를 직접 만드는 대신 factorization/solve를 쓸 수 있는가?
- saturation 이후에도 stability/safety contract가 유지되는가?

교육용 3×3 닫힌형 inverse는 반복 상한을 설명하기 좋지만, 고자유도 제품 코드에서는 pivoting/factorization, condition estimate, tested linear-algebra backend가 보통 더 적합하다.
