# 논문 리뷰 — Control of Redundant Robots Under Hard Joint Constraints: Saturation in the Null Space

## 서지 정보

- Fabrizio Flacco, Alessandro De Luca, Oussama Khatib
- *IEEE Transactions on Robotics*, Vol. 31, No. 3, pp. 637–654, 2015
- DOI: [10.1109/TRO.2015.2418582](https://doi.org/10.1109/TRO.2015.2418582)
- 공개 서지 및 postprint: [Sapienza IRIS](https://hdl.handle.net/11573/780461)
- 선행 ICRA 2012 논문: [Motion Control of Redundant Robots under Joint Constraints: Saturation in the Null Space](https://www.diag.uniroma1.it/~labrob/pub/papers/ICRA12_RedundancySNS.pdf)

## 1. 해결하려는 문제

중복 로봇은 task 차원보다 관절 수가 많다. 예를 들어 7축 팔로 6차원 end-effector 속도를 만들면 적어도 한 방향의 null space가 남는다. 보통 pseudoinverse의 최소 norm 해를 쓰지만, 그 해의 한 관절이 velocity/acceleration/position-derived bound를 넘을 수 있다.

명령을 계산한 뒤 각 관절을 단순 clamp하면 hard bound는 지켜도 `J q_dot`가 원래 Cartesian 방향에서 벗어난다. 충돌 회피처럼 방향이 중요한 primary task에서는 “제한을 지켰지만 엉뚱한 방향으로 움직이는” 결과가 된다. 반대로 전체 task를 처음부터 크게 축소하면 실제로는 redundancy로 해결할 수 있었던 성능까지 버린다.

논문의 질문은 명확하다.

> 관절 한계는 절대 위반하지 않되, 먼저 중복 자유도를 최대한 사용하고, 그래도 불가능할 때만 원래 task 방향을 유지하면서 최소한으로 scale할 수 있는가?

## 2. 핵심 아이디어와 수학적 직관

### 2.1 Saturation을 “사후 clamp”가 아니라 active set으로 취급

기본 최소 norm 해에서 한계를 넘는 관절을 발견하면 그 관절을 사용 불가로 버리는 것이 아니라, 정확히 상한 또는 하한 속도에 고정한다. 고정된 관절이 이미 만드는 task-space 속도를 우변에서 빼고, 남은 자유 관절의 pseudoinverse로 부족분을 다시 만든다.

```text
J q_dot = x_dot_d
q_dot = q_N + W q_dot_free
J W q_dot_free = x_dot_d - J q_N
```

여기서 `W`는 아직 자유로운 관절만 선택하고 `q_N`은 이미 포화된 관절의 속도다. 한 관절을 포화할 때마다 새로운 null space에서 다시 풀기 때문에, 단순 clamp보다 primary task를 유지할 가능성이 크다.

### 2.2 Task scaling은 최후의 수단

남은 자유도로도 원래 `x_dot_d`를 만들 수 없으면 다음처럼 방향을 유지한 scale `s`를 찾는다.

```text
J q_dot = s x_dot_d,  0 <= s <= 1
```

한 active set에서 `q_dot(s)=a s+b`이므로 각 joint bound는 `s`에 대한 interval이 된다. 모든 interval의 교집합에서 가장 큰 `s`를 선택하면 “가능한 만큼 가장 빠르게” 같은 Cartesian 방향으로 움직인다.

### 2.3 우선순위 task와 QP 관점

논문은 단일 task에 그치지 않고 여러 task의 preemptive priority를 유지하는 SNS와, 관련 Quadratic Programming 문제를 통해 optimality를 분석한 변형까지 제시한다. 중요한 엔지니어링 메시지는 soft penalty 가중치를 어렵게 조정하는 대신, 높은 우선순위 task와 hard constraint를 구조적으로 분리한다는 점이다.

### 2.4 계산량이 작은 이유

매 반복에서 pseudoinverse/작은 행렬 solve와 한계 검사를 하고, 포화될 수 있는 관절 수가 유한하므로 반복 상한이 관절 수에 묶인다. 저자들은 많은 hard bound와 우선순위 task를 온라인/실시간 제어에 사용할 수 있도록 수치 효율적인 버전을 제안하고 실험으로 보였다. 다만 “논문 실험에서 빠름”과 특정 제품의 hard-WCET 보장은 같은 주장이 아니다.

## 3. 실무 적용 가능성

### 잘 맞는 곳

- 7축 협동로봇에서 end-effector 속도를 유지하면서 joint velocity/position margin을 지켜야 할 때
- humanoid/whole-body control에서 높은 우선순위 안전 task와 낮은 우선순위 자세 task를 함께 다룰 때
- joint별 서로 다른 속도/가속도 한계가 있고 단순 global scaling의 성능 손실이 큰 경우
- 충돌 회피처럼 task 방향 일관성이 중요한 온라인 local controller

### 제품 코드로 옮길 때 필요한 것

1. `q`, `q_dot`, 제어주기 `dt`에서 position/velocity/acceleration 허용 구간을 같은 수준의 bound로 변환한다.
2. pseudoinverse 구현의 rank threshold, damping, scaling, 단위를 명시한다.
3. active set, task scale, bound margin, residual, condition estimate, solve time을 진단에 남긴다.
4. NaN, stale state, rank loss, infeasible task의 유한한 fallback을 둔다.
5. actuator 직전 독립 safety monitor가 최종 명령의 hard bound를 다시 확인한다.

## 4. 한계와 주의점

- local differential kinematics이므로 미래 경로의 dead end나 joint limit 접근을 장기적으로 예측하지 않는다.
- Jacobian model/calibration이 틀리면 수학적으로 feasible한 명령도 실제 Cartesian 움직임은 다를 수 있다.
- 특이점에서 pseudoinverse와 task scaling이 민감해진다. damping은 폭주를 줄이지만 exact task 달성을 포기한다.
- active-set 전환은 명령의 연속성/jerk를 악화시킬 수 있어 가속도 수준 SNS, rate limit, 필터 설계가 필요하다.
- multi-task hierarchy가 길어지면 각 task의 rank와 feasibility, numerical tolerance가 복잡해진다.
- 원 논문의 “hard constraint”는 알고리즘 출력의 수학적 bound다. 메시지 지연, actuator saturation 오차, low-level tracking까지 자동으로 보장하지 않는다.

## 5. 오늘 구현과 논문의 차이

| 항목 | 논문 | 오늘의 교육용 패키지 |
|---|---|---|
| 로봇/작업 | 일반 중복 로봇, 단일·다중 우선순위 task | 3R 평면 팔, 2D 속도 task 1개 |
| 제약 | position/velocity/acceleration 등 hard joint bound | 고정 대칭 joint velocity bound |
| 알고리즘 | basic/optimal SNS와 효율적 변형 | 최대 3회 greedy basic SNS + DLS |
| 실험 | 실제 로봇을 포함한 성능 검증 | Euler 적분 teaching plant |
| 안전 증거 | 알고리즘/실험 분석 | 독립 ROS 노드의 sequence·limit·residual 재검산 |

따라서 이 코드는 논문 전체의 재현물이 아니다. 핵심 사고방식인 “redundancy 우선, saturation을 active set으로, scaling은 마지막”을 작은 고정 크기 커널로 읽고 검증하기 위한 뼈대다.
