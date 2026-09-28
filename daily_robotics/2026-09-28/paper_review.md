# 논문 리뷰 — Constraint-consistent Task-oriented Whole-body Robot Formulation

## 서지 정보

- Oussama Khatib, Mikael Jorda, Jaeheung Park, Luis Sentis, Shu-Yun Chung 외
- **“Constraint-consistent task-oriented whole-body robot formulation: Task, posture, constraints, multiple contacts, and balance”**
- *The International Journal of Robotics Research*, Vol. 41, Issue 13–14, 2022
- DOI: [10.1177/02783649221120029](https://doi.org/10.1177/02783649221120029)
- 출판사 [초록/서지 페이지](https://journals.sagepub.com/doi/10.1177/02783649221120029)

이 논문은 1987년 operational-space formulation을 그대로 반복하는 글이 아니다. 고차원·부유기저·다중접촉 로봇에서 작업, 자세, 접촉 제약, 내부력, 균형을 한 수학적 언어로 묶고 실제 humanoid로 검증한 현대적 통합판이다.

## 1. 해결하려는 문제

로봇 팔 하나가 자유공간에서 손끝만 움직일 때는 `x=f(q)`와 `J(q)`만으로도 설명이 비교적 쉽다. 그러나 humanoid는 다음을 동시에 만족해야 한다.

- 발 접촉은 미끄러지거나 떨어지지 않아야 한다.
- 손은 목표 pose/force를 따라야 한다.
- 무게중심과 균형을 유지해야 한다.
- 남는 자유도로 자연스러운 자세와 관절 한계를 관리해야 한다.
- 여러 접촉이 만드는 **합력(resultant force)**과 로봇 내부에서만 순환하는 **내부력(internal force)**을 구분해야 한다.

단순히 각 목표의 관절 토크를 더하면 서로 싸운다. 특히 부유기저 humanoid는 베이스를 직접 구동할 수 없고, 접촉이 바뀔 때 허용되는 운동공간도 바뀐다. 논문의 질문은 “각 목표를 독립적으로 설계하면서도 실제 동역학과 접촉 제약 아래 하나의 일관된 whole-body torque로 합칠 수 있는가?”이다.

## 2. 핵심 아이디어와 수학적 직관

### 2.1 작업을 관절이 아니라 의미 있는 공간에 쓴다

손 위치, 발 접촉, 무게중심처럼 엔지니어가 실제로 원하는 변수를 `x`로 두고 `x_dot=J q_dot`로 관절과 연결한다. 작업공간이 느끼는 유효 관성은 다음 꼴이다.

```text
Lambda = (J A^-1 J^T)^-1
```

여기서 `A`는 전체 generalized inertia다. `Lambda`는 “손끝 x 방향으로 같은 가속도를 만들 때 로봇 자세에 따라 얼마나 무겁게 느껴지는가”를 나타낸다. 관절별 독립 gain보다 작업 좌표에서의 응답을 직접 설계할 수 있다.

### 2.2 제약을 먼저 투영하고 그 안에서 작업한다

발이 바닥에 고정되었다면 모든 generalized velocity/acceleration이 허용되는 것이 아니다. support Jacobian의 동적 일관 null space로 운동과 토크를 투영해 접촉을 깨지 않는 부분공간만 남긴다. 그 뒤 손 작업 Jacobian도 이 제약공간에 맞게 다시 정의한다.

직관적으로는 다음 순서다.

```text
전체 자유도
  └─ 발 접촉을 깨는 방향 제거
       └─ 손 작업에 필요한 방향 할당
            └─ 남은 방향에 자세/추가 작업 할당
```

동적 일관 generalized inverse는 단순 Moore–Penrose inverse와 달리 관성 `A`를 metric으로 쓴다. 그래서 낮은 우선순위 토크를 null space로 보냈을 때 높은 우선순위 작업의 force/acceleration에 결합되지 않는 성질을 얻는다.

### 2.3 multi-contact의 내부력을 따로 다룬다

양발로 바닥을 누르면 같은 몸통 합력을 만드는 접촉력 조합이 여러 개다. 그중 서로 상쇄되어 몸 전체 운동은 만들지 않지만 발 사이 압착이나 접촉 안정성에 영향을 주는 성분이 내부력이다. 논문은 Virtual Linkage Model로 이 성분을 명시적으로 표현하고, 마찰·접촉 안정성을 위한 potential barrier와 연결한다.

### 2.4 coordinate completion

중복된 작업 좌표를 그대로 쓰면 task inertia가 singular해지거나 역행렬 의미가 흐려질 수 있다. 논문은 작업을 완전한 generalized coordinate 표현으로 보완해 redundancy를 제거하면서 전체 operational-space dynamics를 유지하는 coordinate completion을 제안한다. 이는 “무조건 작은 damping을 넣어 역행렬을 통과시키는 것”보다 모델 구조를 더 충실히 보존하려는 접근이다.

## 3. 실무 적용 가능성

### 적용하기 좋은 곳

- torque-controlled humanoid/legged robot의 손 작업 + 균형 + 자세 제어
- mobile manipulator의 베이스/팔 협응
- 양손 조립이나 multi-contact manipulation의 내부력 관리
- 접촉 조건이 분명하고 rigid-body model과 torque sensing 품질이 좋은 연구 플랫폼

### 오늘 코드와의 연결

오늘 실습은 논문 전체의 아주 작은 절단면이다.

| 논문 | 오늘 실습 |
|---|---|
| 부유기저 + multi-contact | 고정베이스 3R 평면 팔 |
| support-consistent task Jacobian | 단일 2D 말단 Jacobian |
| virtual linkage/internal force | 구현하지 않음 |
| coordinate completion | 작은 determinant에 damping 적용 |
| 여러 task/constraint hierarchy | 말단 1순위 + 자세 2순위 |
| 실제 humanoid 검증 | 결정론적 교육용 simulation plant |

그럼에도 핵심 불변식은 같다. 오늘의 `Lambda=(JM^-1J^T)^-1`, `J_bar=M^-1J^T Lambda`, `N^T tau_0`가 논문의 constraint/task hierarchy로 확장되는 씨앗이다.

## 4. 한계와 엔지니어링 주의점

1. **모델 오차:** link inertia, 마찰, payload, 접촉 compliance가 틀리면 동적 decoupling이 깨진다. disturbance observer나 robust/adaptive layer가 필요하다.
2. **접촉은 이상적이지 않다:** 실제 발/그리퍼는 유한 면적, 마찰 uncertainty, 미끄럼, 충격을 가진다. rigid constraint만으로 안전을 보장할 수 없다.
3. **특이점/랭크 전환:** 접촉 추가·해제나 Jacobian rank 변화에서 projector가 불연속적으로 바뀌면 큰 토크가 생길 수 있다. transition smoothing과 torque-rate limit이 필요하다.
4. **토크/관절 한계:** 순수 null-space 투영은 actuator saturation, joint limits, collision 같은 부등식 제약을 자연스럽게 강제하지 않는다. hierarchical QP/SNS가 실무에서 자주 필요하다.
5. **계산시간:** 수식이 우아해도 고자유도 모델 업데이트, 분해, 접촉 변경이 deadline 안에 끝난다는 보장은 별도다. WCET 계측과 fallback supervisor가 필요하다.
6. **안전 독립성:** software whole-body controller는 STO, 브레이크, collision monitor, watchdog을 대체하지 않는다.

## 엔지니어의 한 줄 결론

이 논문의 진짜 가치는 “토크 항을 여러 개 더하는 요령”이 아니라, **접촉이 허용한 동역학 공간 안에서 높은 우선순위 작업을 먼저 정의하고 남은 운동/힘 공간을 낮은 우선순위에 일관되게 양도하는 설계 원칙**에 있다.
