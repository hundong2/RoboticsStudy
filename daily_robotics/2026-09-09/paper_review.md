# 논문 리뷰 — Dynamic Locomotion in the MIT Cheetah 3 Through Convex Model-Predictive Control

## 서지 정보

- **저자:** Jared Di Carlo, Patrick M. Wensing, Benjamin Katz, Gerardo Bledt, Sangbae Kim
- **학회:** 2018 IEEE/RSJ International Conference on Intelligent Robots and Systems (IROS)
- **DOI:** [10.1109/IROS.2018.8594448](https://doi.org/10.1109/IROS.2018.8594448)
- **원문:** [MIT Open Access record](https://dspace.mit.edu/handle/1721.1/138000)
- **선정 이유:** MPC의 추상적인 “미래를 보고 최적화한다”는 설명이 실제 torque-controlled 사족보행 로봇의 3D 동역학, 접촉 제약, 1 ms 미만 풀이, 다양한 gait로 이어지는 과정을 보여주는 대표적인 로보틱스 시스템 논문이다.

## 1. 해결하려는 문제

사족보행 로봇은 어느 발이 지면에 닿아 있는지가 계속 바뀌는 **hybrid system**이다. 몸통의 위치·자세를 원하는 궤적으로 보내려면 현재 힘만 계산해서는 부족하다. 곧 발이 떨어질지, 다음 접촉에서 어느 방향으로 얼마만큼의 지면 반력을 낼 수 있을지까지 봐야 한다.

전신 비선형 동역학과 접촉을 그대로 최적화하면 모델은 정확하지만 온라인 계산이 너무 비싸다. 반대로 단순 PD 제어만 쓰면 계산은 빠르지만 여러 접촉에 힘을 어떻게 배분할지, 마찰과 unilateral contact를 어떻게 만족할지 체계적으로 다루기 어렵다.

논문의 질문은 실무적으로 다음 한 문장이다.

> **온보드 계산 예산 안에서 미래 접촉을 고려하면서도, 실제 Cheetah 3가 빠르고 다양한 gait로 움직일 만큼 유용한 지면 반력을 어떻게 계산할 것인가?**

## 2. 핵심 아이디어와 수학적 직관

### 2.1 정확한 모든 것을 풀지 말고, 제어에 중요한 몸통 동역학을 남긴다

저자들은 로봇의 몸통을 하나의 강체로 근사하고 다리의 질량 등 일부 효과를 단순화한다. 각 접촉 발의 지면 반력은 몸통의 선형 가속도와 각가속도를 만든다.

직관적으로는 다음 두 식이다.

\[
m\dot{v}=\sum_i f_i + mg
\]

\[
I\dot{\omega}=\sum_i r_i \times f_i
\]

여기서 `f_i`는 i번째 발의 지면 반력, `r_i`는 질량중심에서 발까지의 lever arm이다. 힘의 합은 몸통을 이동시키고, `r × f`의 합은 몸통을 회전시킨다.

### 2.2 미래의 접촉 스케줄을 따라 힘 열(sequence)을 한 번에 고른다

MPC는 현재 한 시점의 힘만 고르지 않는다. 정해진 예측 구간에서 각 발이 stance인지 swing인지 반영하고, 몸통 상태 오차와 입력 크기를 함께 비용으로 둔다.

\[
\min_U \sum_{k=0}^{N-1}
(x_k-x_k^{ref})^TQ(x_k-x_k^{ref})+u_k^TRu_k
\]

- `Q`가 크면 몸통 위치·속도·자세 reference를 강하게 추종한다.
- `R`이 크면 과도한 지면 반력을 피한다.
- 실제 적용은 최적 힘 열의 첫 부분뿐이고, 다음 제어 시점에 새 상태로 다시 푼다.

이 receding-horizon 구조가 모델 오차와 외란을 매번 피드백으로 보정하는 핵심이다.

### 2.3 물리 제약을 convex QP로 만든다

발은 지면을 당길 수 없으므로 수직 힘은 음수가 될 수 없다. 미끄러지지 않으려면 접선 힘도 마찰 한계 안에 있어야 한다. 마찰 원뿔을 선형 pyramid로 근사하면 다음과 같은 선형 부등식으로 다룰 수 있다.

\[
f_z \ge 0, \qquad |f_x| \le \mu f_z, \qquad |f_y| \le \mu f_z
\]

단순화한 선형 예측 모델, quadratic cost, linear constraint를 결합하면 convex quadratic program(QP)이 된다. convex 문제는 local minimum 함정 없이 빠르고 반복 가능한 solver를 설계하기 유리하다.

### 2.4 “좋은 모델”의 기준은 가장 정확함이 아니라 제어 주기 안에 유용함이다

논문은 최대 약 0.5 s의 horizon을 다루고, 최적화 문제를 1 ms 미만에 풀어 20–30 Hz로 갱신했다고 보고한다. 단순화한 모델인데도 Cheetah 3에서 여러 gait와 3D gallop을 포함한 동작을 보였고, 전진 속도는 최대 3 m/s 수준까지 보고했다.

핵심 교훈은 정확도 자체가 아니라 **모델 충실도 × 계산시간 × 피드백 갱신률**의 균형이다.

## 3. 실무 적용 가능성과 한계

### 적용할 수 있는 부분

1. **계층형 제어:** 상위 MPC가 비교적 낮은 주기로 힘/목표를 만들고, 하위 torque/whole-body controller가 더 빠른 주기로 추종하는 구조는 모바일 매니퓰레이터와 legged robot에 널리 재사용할 수 있다.
2. **제약을 설계 요구사항으로 표현:** torque, friction, contact, slew-rate를 사후 clamp가 아니라 최적화 문제 안에 넣으면 “가능한 명령 중 가장 좋은 것”을 고를 수 있다.
3. **고정 horizon과 측정 가능한 실행시간:** 평균 속도보다 worst-case latency와 deadline miss를 기록해야 실제 제어 주기에 넣을 수 있다.
4. **모델 단순화의 의도적 사용:** 모든 유연성·백래시·접촉 충격을 넣기보다 온라인 의사결정에 필요한 상태와 제약을 남긴다.

### 한계와 제품화 위험

1. **모델 불일치:** 다리 관성, 비선형 회전 항, 접촉 충격 등 생략한 효과가 커지는 동작에서는 예측이 틀어진다.
2. **접촉 스케줄 의존:** 어느 발이 언제 닿는지가 주어졌다는 전제가 깨지거나 발이 미끄러지면 계획한 힘을 만들 수 없다.
3. **상태 추정 품질:** 몸통 pose/velocity, 접촉 상태, 지면 normal이 늦거나 noisy하면 QP가 빨리 풀려도 잘못된 입력을 낸다.
4. **마찰 모델 오차:** 실제 `mu`가 설정값보다 낮으면 수학적으로 feasible한 힘도 물리적으로 미끄러진다.
5. **평균 solve time은 RT 보장이 아님:** 1 ms 미만 결과와 hard deadline 보장은 다르다. allocator, cache, scheduler, priority inversion, solver의 worst-case iteration을 별도로 검증해야 한다.
6. **MPC는 safety controller가 아님:** 최적화 infeasible, stale state, NaN, 통신 단절에 대비한 독립 watchdog과 hardware limit가 필요하다.

## 오늘 실습과의 연결

| 논문의 Cheetah 3 | 오늘의 1축 실습 |
|---|---|
| 3D 몸통 상태와 여러 발의 접촉력 | `[theta, omega]`와 모터 토크 하나 |
| 접촉/마찰 제약이 있는 convex QP | torque/slew 제약이 있는 projected-gradient |
| 약 0.5 s 예측 | 0.1 s 예측(20 × 5 ms) |
| 첫 힘을 적용하고 다시 최적화 | `torque_sequence_[0]`만 발행하고 5 ms 뒤 재계산 |
| 실제 로봇 모델 불일치 | Controller는 중력을 생략하지만 Plant는 `sin(theta)` 포함 |
| 실시간 계산이 제품 성능의 일부 | 고정 12회 반복과 callback 시간 histogram |

오늘 코드는 논문의 solver나 전신 모델을 재현하지 않는다. 대신 MPC의 최소 골격인 **예측 모델, finite horizon, 비용, 입력 제약, warm start, 첫 입력 적용, 다시 풀기**를 작은 시스템에서 분리해 볼 수 있게 한다.

## 엔지니어를 위한 확인 질문

1. horizon을 늘리면 제어 품질이 항상 좋아지지 않는 이유는 무엇인가?
2. friction cone을 pyramid로 바꾸면 계산과 물리 정확도에 어떤 trade-off가 생기는가?
3. QP solve time이 평균 0.5 ms여도 1 kHz loop에 바로 넣으면 안 되는 이유는 무엇인가?
4. 상태 추정이 20 ms 늦는다면 모델, 비용, 제약 중 무엇을 먼저 점검할 것인가?
5. 오늘 실습의 사후 Plant clamp와 MPC 내부 제약은 각각 어떤 실패를 막는가?
