# 논문 리뷰 — Operational Space Formulation

## 선정 논문

Oussama Khatib, **“A Unified Approach for Motion and Force Control of Robot Manipulators: The Operational Space Formulation,”** *IEEE Journal of Robotics and Automation*, Vol. RA-3, No. 1, pp. 43–53, February 1987.

- 원문: [Stanford Robotics Lab PDF](https://khatib.stanford.edu/publications/pdfs/Khatib_1987_RA.pdf)
- DOI: [10.1109/JRA.1987.1087068](https://doi.org/10.1109/JRA.1987.1087068)
- 선정 이유: 로봇의 실제 임무가 정의되는 end-effector 공간에서 운동과 힘을 함께 다루는 기반을 세웠다. 오늘 구현한 `wrench → contact force → joint torque` 흐름을 이해하는 가장 직접적인 근간 논문이다.

## 1. 해결하려는 문제

관절각을 잘 추종하는 로봇이 반드시 작업을 잘하는 것은 아니다. 문을 미는 임무는 손끝 힘이 중요하고, 표면을 따라 닦는 임무는 접선 방향의 운동과 법선 방향의 힘을 동시에 제어해야 한다. 여유 자유도가 있는 로봇은 손의 임무를 유지하면서 팔꿈치 자세나 장애물 회피도 처리해야 한다.

당시의 전형적인 joint-space 관점만으로는 다음 질문이 간접적이었다.

- end-effector가 방향마다 느끼는 유효 관성은 얼마인가?
- 구속된 방향의 접촉력과 자유로운 방향의 운동을 한 식에서 어떻게 다루는가?
- redundant manipulator의 여분 자유도를 주 임무를 방해하지 않고 어떻게 쓰는가?
- singularity 근처에서 실제로 제어 가능한 작업 방향은 무엇인가?

논문의 핵심 문제는 로봇 동역학을 “모터 관절의 관점”에서만 보지 않고, **임무가 표현되는 operational space의 동역학으로 직접 옮기는 것**이다.

## 2. 핵심 아이디어와 수학적 직관

### 2.1 관절 동역학에서 작업공간 동역학으로

일반적인 관절 동역학을 단순화해 쓰면 다음과 같다.

```text
M(q) q_ddot + b(q, q_dot) + g(q) = tau
```

end-effector 속도는 `x_dot = J(q) q_dot`다. 이 관계와 관절 동역학을 결합하면 작업공간에서 다음 형태의 운동 방정식을 만들 수 있다.

```text
Lambda(q) x_ddot + mu(q, q_dot) + p(q) = F

Lambda = (J M^-1 J^T)^-1
```

`Lambda`는 단순히 관절 질량행렬을 좌표만 바꾼 것이 아니라, end-effector가 각 방향에서 느끼는 **operational-space inertia**다. 같은 로봇 자세에서도 손을 밀려는 방향에 따라 무겁거나 가볍게 느껴지는 이유를 이 행렬이 설명한다.

### 2.2 힘은 Jacobian transpose로 관절에 전달된다

작업공간에서 원하는 generalized force가 `F`이면 관절 토크 기여는 다음 관계를 갖는다.

```text
tau_task = J^T F
```

직관은 가상일 보존이다. `dx=J dq`일 때 `F^T dx = tau^T dq`여야 하므로 `tau=J^T F`가 된다. 오늘 실습의 auditor가 양발 접촉력으로부터 관절 effort를 계산하는 부분이 이 연결의 가장 작은 예다.

### 2.3 운동과 힘의 통합

환경이 운동을 허용하는 방향과 막는 방향을 나누면, 자유 방향에는 motion control을, 구속 방향에는 force control을 적용할 수 있다. 논문의 중요한 기여는 이를 ad-hoc한 관절 제어 조합이 아니라 operational space의 동역학과 selection 구조 안에서 체계화한 것이다.

### 2.4 redundancy와 singularity

여유 자유도는 “풀어야 할 골칫거리”만이 아니라, 주 end-effector 임무에 영향을 주지 않는 부분공간에서 부가 임무를 수행할 자원이다. 현대 whole-body control에서 task/null-space hierarchy가 널리 쓰이는 철학적·수학적 기반이 여기에 있다.

singularity에서는 모든 operational direction을 동일하게 제어할 수 없다. 논문은 제어 가능한 부분공간과 singular direction을 구분해 다루며, 단순히 역행렬 계산이 실패하는 수치 문제로만 보지 않는다.

## 3. 오늘 코드와의 연결

오늘 코드는 논문의 전체 동역학 제어기를 재현하지 않는다. 대신 제품 시스템에서 자주 쓰는 한 하위 문제를 분리한다.

```text
desired operational/centroidal wrench
            ↓
contact allocation:  min ||Af - w_des||²
            ↓
foot contact forces f
            ↓
joint effort: tau = J^T f
```

논문의 `F`가 end-effector task force를 뜻한다면, 오늘 `w_des`는 질량중심 기준 로봇 전체 wrench다. QP는 이를 두 발의 물리 가능한 접촉력으로 나눈다. 그 뒤 각 다리 Jacobian transpose가 발 힘을 관절 effort로 옮긴다.

이 분리를 통해 다음을 배울 수 있다.

- operational task를 먼저 정의하면 actuator 수가 바뀌어도 임무 의미를 유지하기 쉽다.
- 동일한 net wrench를 만드는 접촉력 해가 여러 개일 수 있어 regularization과 안전 제약이 필요하다.
- `J^T f`는 힘 전달 관계이지, 그 자체가 완전한 동역학 보상 제어기는 아니다.
- 수학적 feasible과 실물 안전은 다르므로 독립 monitor와 actuator/contact 한계가 필요하다.

## 4. 실무 적용 가능성

이 철학은 다음 시스템에서 직접 살아 있다.

- 사족/휴머노이드의 centroidal wrench 및 ground-reaction-force 분배
- 산업용 조작기의 hybrid motion/force control
- redundant arm의 task-priority whole-body control
- mobile manipulator의 base/arm 협조 제어
- 접촉 작업에서 impedance/admittance와 inverse dynamics를 결합한 제어기

제품 구현에서는 상위 task controller, state estimator, contact estimator, QP/WBC, torque controller, independent safety supervisor를 분리하는 편이 디버깅과 인증에 유리하다. 오늘 세 노드 구조는 그 경계를 작게 모사한다.

## 5. 한계와 엔지니어링 주의점

### 모델 정확도

`Lambda`, Coriolis/centrifugal 항, 중력, Jacobian이 정확해야 높은 대역폭의 operational-space inverse dynamics가 의도대로 동작한다. payload, 케이블, 관절 마찰, 유연성, 접촉 강성이 틀리면 force overshoot나 tracking error가 생긴다.

### 접촉은 이상적인 holonomic constraint가 아니다

실물 발은 미끄러지고, 표면은 변형되며, 충돌에는 순간적인 impulse가 있다. 마찰계수도 상수가 아니다. 오늘의 `|Fx| <= mu Fz`는 교육용 평면 모델이고, 실기는 friction uncertainty, CoP, torsional moment, contact transition을 다뤄야 한다.

### singularity와 수치 조건

`J M^-1 J^T`가 나쁘게 conditioned되면 `Lambda` 계산이 민감해진다. damping, SVD/QR, task reduction, posture change가 필요하며, “역행렬이 계산됐다”를 안전 판정으로 써서는 안 된다.

### 계산 시간과 수렴

논문의 수학적 제어 법칙과 실시간 실행 보장은 별개다. 현대 whole-body QP도 문제 크기, active set, warm start, 메모리 할당, solver failure 상태에 따라 시간이 변한다. 그래서 오늘 코드는 정확히 64회만 계산하고 residual/feasibility/time을 밖으로 내보낸다. 다만 이 또한 WCET 분석을 대신하지 않는다.

### 안전 독립성

같은 프로세스와 같은 모델이 만든 “self check”는 공통 원인 오류를 놓칠 수 있다. 실제 안전 시스템은 별도 프로세서, 단순한 한계 규칙, torque/velocity/energy monitor, hardware stop을 조합해야 한다. 오늘 auditor는 개념적 독립 재계산일 뿐 안전 인증 구성요소가 아니다.

## 엔지니어의 한 줄 결론

이 논문의 가장 오래가는 가치는 특정 제어식 하나가 아니라, **로봇이 해야 할 일을 task space의 운동과 힘으로 먼저 표현하고, 동역학·여유 자유도·접촉을 그 임무 아래 조직하라**는 설계 관점이다. 오늘 QP는 그 관점을 bounded하고 관찰 가능한 소프트웨어 경계로 옮긴 작은 예다.
