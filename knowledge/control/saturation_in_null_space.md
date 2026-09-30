# Saturation in the Null Space (SNS)

## 문제 형태

중복 로봇의 velocity task와 box joint bound를 생각한다.

```text
J(q) q_dot = x_dot_d
q_dot_min <= q_dot <= q_dot_max
```

최소 norm pseudoinverse 해를 구한 뒤 clamp하면 bound는 맞지만 Cartesian task 방향이 깨진다. SNS는 위반 joint를 한계에 고정하고 남은 null space에서 task를 다시 풀어 redundancy를 먼저 쓴다.

## Active-set 표현

`W`는 자유 관절을 선택하는 diagonal matrix, `q_N`은 포화 관절의 고정 속도다.

```text
q_dot(s) = q_N + W J^T (J W J^T + lambda^2 I)^-1 (s x_dot_d - J q_N)
         = a s + b
```

각 관절 bound는 scale `s`의 구간이 된다. 구간 교집합의 최대 `s∈[0,1]`를 현재 active set의 실행 가능 성능으로 저장한다. `s=1` 해가 위반하면 가장 심한 joint를 고정하고 반복한다. 남은 Jacobian rank가 task 차원보다 작아지면 저장한 최선 해를 사용한다.

## 단순 clamp와의 차이

```text
post clamp: q_dot = clamp(J^+ x_dot_d)       # J q_dot 방향이 바뀔 수 있음
global scale: q_dot = s J^+ x_dot_d          # redundancy를 덜 활용할 수 있음
SNS: saturate one joint, resolve remainder    # task를 먼저 보존
```

## 구현 체크리스트

- joint별 position/velocity/acceleration bound를 현재 `q`, `q_dot`, `dt`로 같은 level에 통합한다.
- rank/condition과 damping을 기록한다.
- 최대 반복 수를 joint 수로 제한하고 memory 크기를 고정한다.
- `task_scale`, `saturated_mask`, task residual, solve time을 공개한다.
- active-set 전환에서 가속도/jerk가 커지는지 별도 검사한다.
- stale/NaN/infeasible 입력의 finite fallback을 둔다.
- actuator 직전 독립 경로가 최종 joint bound를 다시 확인한다.

## 확장

- multi-task: 높은 우선순위 task가 만든 null space 안에서 다음 task를 푼다.
- optimal SNS: 연관 QP의 optimality를 만족하도록 saturation 선택을 개선한다.
- generalized constraints: joint box뿐 아니라 Cartesian inequality를 함께 다룬다.
- acceleration/torque level: dynamics와 actuator rate/torque bound를 포함한다.

## 참고

- Flacco, De Luca, Khatib, *IEEE TRO* 2015, DOI: `10.1109/TRO.2015.2418582`
- 구현 실습: `daily_robotics/2026-10-01`
- 기초 null space: `knowledge/control/operational_space_nullspace.md`
