# Model Predictive Control (MPC)

## 한 문장 정의

MPC는 **현재 상태에서 미래를 일정 구간 예측하고, 비용과 제약을 만족하는 입력 열을 최적화한 뒤, 첫 입력만 적용하고 다음 주기에 다시 푸는 제어 방식**이다.

## 기본 구성

1. **상태 `x`:** 제어에 필요한 위치, 속도, 자세, 온도 등
2. **입력 `u`:** torque, steering, thrust, ground reaction force 등
3. **예측 모델:** `x[k+1] = f(x[k], u[k])`
4. **reference:** 원하는 상태 또는 trajectory
5. **비용:** tracking error, 입력 크기, 입력 변화율을 숫자로 평가
6. **제약:** torque, 속도, collision, friction, workspace처럼 넘으면 안 되는 경계
7. **horizon:** 몇 step 앞을 볼지 결정하는 `N`

## Receding horizon

최적 입력 열이 `[u0, u1, ..., uN-1]`이어도 `u0`만 적용한다. 그 뒤 센서를 다시 읽고 문제를 다시 푼다.

이 구조의 장점:

- 새 외란과 상태 추정값을 매 주기 반영한다.
- 모델 오차가 끝없이 누적되는 open-loop plan을 피한다.
- 미래 제약을 미리 보고 현재 입력을 완만하게 바꿀 수 있다.

한계:

- solver deadline을 놓치면 제어 입력도 늦어진다.
- 잘못된 모델과 상태 추정으로 푼 최적해는 현실에서 최적이 아니다.
- infeasible/NaN/stale 입력을 처리할 fallback이 별도로 필요하다.

## Projected-gradient 학습 뼈대

`daily_robotics/2026-09-09`는 다음 순서로 동작한다.

```text
shift previous solution        # warm start
repeat a fixed 12 times:
    roll out N states          # x[k+1] = A*x[k] + B*u[k]
    back-propagate adjoint     # dJ/du를 O(N)에 계산
    u <- u - alpha*dJ/du       # gradient step
    project u onto constraints # torque + slew-rate limit
publish u[0]
```

`std::array`와 고정 반복은 계산·메모리 상한을 이해하기 쉽다. 다만 일반적인 box/slew projection을 연속으로 적용한 결과가 복합 feasible set에 대한 정확한 Euclidean projection이라고 항상 보장되지는 않는다. 제품에서는 검증된 QP/NLP solver, convergence 검사, infeasibility 처리와 fallback을 사용한다.

## 튜닝 방향

- position weight 증가: 빠른 추종을 선호하지만 torque/saturation이 커질 수 있음
- velocity weight 증가: overshoot와 진동 억제, 대신 응답이 느려질 수 있음
- input weight 증가: torque를 절약하고 부드러워지나 tracking error 증가
- horizon 증가: 더 먼 제약을 보지만 계산량과 모델 오차 노출 증가
- iteration 증가: 수렴 가능성은 높아지지만 worst-case 실행시간 증가

평균 tracking error만 보지 말고 `max solve time`, deadline misses, constraint violation, saturation duration, fallback 횟수를 함께 기록한다.
