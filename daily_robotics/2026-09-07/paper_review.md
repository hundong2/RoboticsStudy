# 논문 리뷰 — Dynamic Window Approach

## 서지 정보

- Dieter Fox, Wolfram Burgard, Sebastian Thrun
- **“The Dynamic Window Approach to Collision Avoidance”**
- *IEEE Robotics & Automation Magazine*, 4(1), 23–33, 1997
- DOI: [10.1109/100.580977](https://doi.org/10.1109/100.580977)
- 원 저자 소속 기관의 서지·초록: [Carnegie Mellon Robotics Institute](https://publications.ri.cmu.edu/the-dynamic-window-approach-to-collision-avoidance)

## 1. 해결하려는 문제

이동로봇의 지역 회피기는 수십~수백 ms마다 새 속도 명령을 내야 한다. 단순히 지도 위에서 충돌 없는 방향을 골라도 모터와 로봇 관성 때문에 그 속도로 즉시 바뀔 수 없고, 장애물을 발견한 뒤 제동할 공간이 부족할 수도 있다. 1990년대의 많은 회피 방식은 주로 위치공간의 기하를 다뤘고 실제 구동계의 속도·가속도 한계를 제어 명령 선택에 직접 연결하기 어려웠다.

Fox, Burgard, Thrun이 묻는 질문은 다음이다.

> 현재 속도와 가속도 한계를 고려해 바로 도달할 수 있고, 충돌 전에 멈출 수 있는 속도 명령만 빠르게 검색할 수 있는가?

논문은 synchro-drive 로봇 RHINO에서 이 접근을 검증했고, 저자 공개 초록은 사람이 있는 동적 환경에서 최대 95 cm/s 주행을 보고한다. 중요한 기여는 “좋은 경로를 먼저 만든 뒤 추종”하는 장거리 계획이 아니라, **짧은 시간 동안 안전하게 실행할 `(v,w)`를 velocity space에서 직접 고르는 지역 제어**다.

## 2. 핵심 아이디어와 수학적 직관

### 위치가 아니라 속도를 검색한다

차동구동/동기구동 로봇이 짧은 구간 동안 일정한 선속도 `v`와 각속도 `w`를 유지하면 궤적은 직선 또는 원호가 된다. 그러므로 후보 하나는 복잡한 경로 배열이 아니라 `(v,w)` 두 숫자로 표현할 수 있다. 각 후보 원호를 장애물과 비교하고 목표 방향, 여유도, 속도를 채점하면 짧은 제어주기 안에 결정을 반복할 수 있다.

### 세 집합의 교집합

DWA의 검색공간은 직관적으로 다음 세 조건의 교집합이다.

```text
V_search = V_limits ∩ V_dynamic ∩ V_admissible
```

- `V_limits`: 모터가 낼 수 있는 절대 선속도·각속도 범위
- `V_dynamic`: 현재 `(v_c,w_c)`에서 한 제어주기 동안 가속해 도달 가능한 범위
- `V_admissible`: 해당 원호에서 가장 가까운 장애물 전에 제동 가능한 속도

동적 창은 다음처럼 현재 속도 주변의 작은 직사각형이 된다.

```text
v_c - a_v Δt ≤ v ≤ v_c + a_v Δt
w_c - a_w Δt ≤ w ≤ w_c + a_w Δt
```

이름의 “window”가 바로 이 움직이는 속도 범위다. 로봇이 빨라지면 창도 속도공간에서 이동한다.

### 제동 가능성

직선 제동 직관만 떼어 보면 최대 감속 `a`에서 정지거리 `d_stop`은 다음이다.

```text
d_stop = v² / (2a)
```

후보 원호를 따라 만나는 첫 장애물까지 거리보다 정지거리와 안전 margin이 작아야 한다. 원 논문은 선속도와 회전속도 양쪽의 admissibility를 속도공간에 연결한다. 오늘 코드는 교육용으로 선형 정지거리와 원형 로봇 반지름을 검사한다. 이 차이는 제품화 때 반드시 보강해야 할 부분이다.

### 목적함수

안전 후보가 여러 개면 다음 성격의 항을 정규화해 합친다.

```text
G(v,w) = α·heading(v,w) + β·clearance(v,w) + γ·velocity(v,w)
```

- `heading`: 후보 원호 끝에서 목표 방향을 얼마나 잘 보는가
- `clearance`: 원호가 장애물에서 얼마나 떨어지는가
- `velocity`: 느리기만 한 해보다 전진 효율이 높은가

수학적으로 어려운 부분보다 엔지니어링에서 더 중요한 것은 세 항의 **단위와 범위를 맞추는 것**이다. clearance가 meter 그대로이고 heading이 radian이면 센서 range만 바뀌어도 가중치 의미가 달라진다. 오늘 코드는 각 항을 대략 `[0,1]`로 제한한 뒤 합산한다.

## 3. 실무 적용 가능성과 한계

### 어디에 유용한가

- 전역 planner가 준 진행 방향을 따르면서 사람·카트·가구를 즉시 피해야 하는 실내 AMR
- 모터 가속도와 최고속도가 명확하고 수십 Hz의 지역 명령이 필요한 차동구동 플랫폼
- 후보 수, rollout step, 장애물 점 수를 고정해 계산량 상한을 관리해야 하는 임베디드 제어기
- 복잡한 최적화 solver보다 디버깅 가능한 score 항과 fail-safe 정지가 우선인 초기 제품

오늘 실습은 Nav2의 DWB controller 전체를 복제하지 않는다. 대신 동적 속도창, 원호 rollout, 제동 gate, 다항 score라는 핵심 뼈대를 105개 고정 후보로 드러낸다. 각 단계의 로그를 남기기 쉬워 센서 좌표계나 비용 정규화 오류를 학습하기 좋다.

### 제품화 전에 확인할 한계

1. **지역 최솟값:** 목표가 장애물 뒤에 있거나 U자형 공간이면 전역 경로 정보 없이 막힐 수 있다.
2. **진동:** 좌우 후보 점수가 비슷하면 매 tick 회전 방향이 바뀐다. hysteresis, oscillation critic, path alignment가 필요하다.
3. **동적 장애물:** 단일 LaserScan snapshot은 사람의 속도를 모른다. tracking과 time-to-collision 예측 없이는 미래 충돌을 놓친다.
4. **Footprint 단순화:** 원형 반지름은 직사각형/비대칭 로봇의 회전 sweep을 부정확하게 본다.
5. **지연 누락:** sensing→planning→actuation latency 동안 이동하는 거리를 제동 margin에 더해야 한다.
6. **샘플 해상도:** 후보를 늘리면 품질은 좋아질 수 있지만 WCET가 선형으로 증가한다. 평균 시간이 아니라 p99와 최대시간을 측정해야 한다.
7. **모델 불일치:** 미끄럼, 경사, payload 변화로 실제 감속이 작아지면 계산된 admissibility가 거짓이 된다.
8. **Executor 비결정성:** 알고리즘이 고정 반복이어도 DDS 수신, parameter service, logging, OS scheduling은 별도 jitter를 만든다.

## 엔지니어의 결론

DWA의 오래가는 가치는 특정 score 공식이 아니라 **기하학적 충돌 회피를 구동계가 실제로 실행 가능한 속도공간 문제로 바꾼 것**이다. 오늘의 구현에서도 가장 먼저 봐야 할 값은 “최고 점수”가 아니라 후보가 왜 탈락했는지, 제동 margin이 실제 장비에서 보수적인지, planner callback이 deadline 안에 끝나는지다.

실무 체크리스트는 네 줄로 압축된다.

1. 현재 속도와 actuator acceleration/deceleration 한계를 실측한다.
2. sensor·planner·actuator latency를 정지거리에 포함한다.
3. 최소 clearance와 command oscillation을 score와 함께 기록한다.
4. 후보 수를 늘리기 전에 worst-case callback 시간을 측정한다.
