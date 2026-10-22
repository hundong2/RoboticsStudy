# Actuator Rate, Power, Energy Supervision

## 세 제한은 다른 위험을 다룬다

- **Rate limit** `|tau_k-tau_(k-1)|/dt <= r_max`: 충격, 구조 진동, command discontinuity를 줄인다.
- **Power limit** `P+=max(0,tau^T q_dot) <= P_max`: 순간적인 양의 기계 에너지 주입률을 제한한다.
- **Energy limit** `integral P+ dt <= E_max`: 순간 power는 작아도 오래 누적되는 heating/interaction 위험을 감시한다.

torque magnitude만 제한하면 빠르게 움직이는 축에서 power가 커질 수 있다. 반대로 power만 제한하면 정지 근처에서 큰 torque가 허용될 수 있으므로 torque/current magnitude limit도 실제 시스템에는 필요하다.

## Leaky bucket

고정 window 합 대신 다음 상태 하나로 최근 에너지 부담을 근사할 수 있다.

```text
E_k = max(0, E_(k-1) - r_recover*dt) + max(0, tau^T q_dot)*dt
E_k <= E_max
```

계산량과 메모리는 O(1)이다. `r_recover`와 `E_max`는 임의 tuning 값이 아니라 motor thermal model, mechanism, task 위험 분석에서 와야 한다.

## 제한 우선순위

위험 power/energy를 즉시 낮추는 emergency projection과 정상 command의 slew limit가 충돌할 수 있다. 많은 시스템에서 안전한 방향의 빠른 감소를 허용하고, 에너지 증가 방향의 slew만 엄격히 제한한다. 어느 정책이든 다음을 문서화한다.

1. 제한 적용 순서와 동시에 만족해야 하는 불변식
2. emergency override 조건
3. saturation 후 integrator anti-windup 경로
4. stale state/NaN/timeout 시 safe state
5. drive-side hardware limit와 상위 supervisor의 책임 분담

## RT 구현 점검

- 고정 크기 scalar/vector 연산, callback 안 allocator/lock/I/O 제거
- measurement timestamp와 command sequence 일치
- steady clock으로 실행시간 측정하되 관찰 최대값을 WCET로 부르지 않기
- supervisor를 best-effort application thread에만 두지 말고 drive/MCU 독립 보호 계층 마련
- power 계산에 사용하는 velocity의 delay, noise, sign convention 검증

## 관련 일일 학습

- `daily_robotics/2026-10-23`: 1-DoF slew/power/leaky-energy supervisor와 독립 invariant audit
