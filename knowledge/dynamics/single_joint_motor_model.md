# 단일 관절 모터 동역학

## 최소 모델

회전 관절 하나를 다음처럼 모델링할 수 있다.

\[
I\ddot{\theta}+b\dot{\theta}+\tau_g(\theta)+\tau_f(\dot{\theta})=\tau_m+\tau_{ext}
\]

- `I`: 모터 rotor, 감속기, 링크를 관절축으로 환산한 등가 관성
- `b`: 점성 마찰 계수
- `tau_g(theta)`: 중력 토크. 단순 링크라면 `mgl sin(theta)` 꼴
- `tau_f`: Coulomb friction, stiction 등 속도 의존 마찰
- `tau_m`: 모터가 낸 제어 토크
- `tau_ext`: 충돌이나 payload가 만드는 외력 토크

`daily_robotics/2026-09-09`의 Plant는 학습에 집중하려고 `I`, 점성 마찰, `sin(theta)` 중력만 사용한다.

## semi-implicit Euler

고정 시간 간격 `dt`에서:

\[
\omega_{k+1}=\omega_k+\ddot{\theta}_k\Delta t
\]

\[
\theta_{k+1}=\theta_k+\omega_{k+1}\Delta t
\]

위치 update에 새 속도를 쓰는 것이 explicit Euler와의 차이다. 에너지 보존계에서 보통 더 안정적이지만, 작은 `dt`와 정확한 접촉 모델을 대신하지는 않는다.

## Controller 모델과 Plant 모델을 분리하는 이유

Controller는 제한 시간 안에 미래를 여러 번 계산해야 하므로 모델을 선형화하거나 항을 생략할 수 있다. Plant/실물에는 그 항이 그대로 남는다. 이 차이가 model mismatch다.

좋은 제어 실험은 mismatch를 숨기지 않는다.

- Controller 모델을 단순화하고 실제 상태 피드백으로 얼마나 보정하는지 본다.
- inertia, damping, payload를 바꿔 robustness를 시험한다.
- 추정 delay, quantization, backlash, saturation을 단계적으로 추가한다.
- 모델이 크게 틀릴 때 독립 safety limit가 동작하는지 확인한다.

## 실제 모터로 옮기기 전 체크

- torque 명령이 실제 drive의 current 명령과 어떤 상수(`K_t`, gear ratio, efficiency)로 연결되는가?
- encoder zero, joint direction, unit(rad/degree), timestamp가 일관적인가?
- software limit 바깥에 drive current limit, mechanical stop, E-stop이 있는가?
- 제어 명령이 stale/NaN/Inf일 때 drive가 어떤 safe state로 가는가?
- worst-case payload와 온도에서 필요한 torque가 continuous rating을 넘지 않는가?
