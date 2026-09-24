# 논문 리뷰 — Time-Optimal Control of Robotic Manipulators Along Specified Paths

## 서지 정보

- J. E. Bobrow, S. Dubowsky, J. S. Gibson, **“Time-Optimal Control of Robotic Manipulators Along Specified Paths”**
- *The International Journal of Robotics Research*, Vol. 4, No. 3, pp. 3–17, 1985
- DOI: [10.1177/027836498500400301](https://doi.org/10.1177/027836498500400301)
- [저자 공개 원문 PDF](https://jebobrow.eng.uci.edu/sites/default/files/IJRR1985.PDF)

이 논문은 최신 논문 대신 선택한 **로봇 궤적 시간매개화의 근간 논문**이다. 오늘 코드처럼 “어떤 기하 경로 또는 관절 이동을 얼마나 빨리 실행할 것인가?”를 다루지만, 단순히 부드러운 다항식을 고르는 데서 멈추지 않고 로봇 동역학과 액추에이터 토크 제한 아래 최소 시간을 찾는다.

## 1. 해결하려는 문제

로봇 end-effector가 따라야 할 공간 경로와 자세가 이미 정해졌다고 하자. 남은 문제는 경로 위에서 시간에 따라 얼마나 빨리 이동할지, 즉 `s(t)`를 정하는 것이다. 너무 느리면 생산성이 떨어지고, 무작정 빠르면 어떤 관절의 요구 토크가 모터 한계를 넘는다.

논문은 다음 조건에서 총 실행 시간을 최소화한다.

- 기하 경로 `q(s)`는 이미 지정되어 있다.
- rigid-link manipulator의 동역학 모델을 알고 있다.
- 관절 위치와 속도에 따라 달라질 수도 있는 토크 상·하한이 있다.
- 최적 open-loop 토크를 구한 뒤 일반적인 선형 feedback으로 오차를 보정한다.

중요한 분리는 **path planning**과 **time parameterization**이다. 장애물을 피하는 경로를 찾는 문제와 그 경로를 토크 한계 안에서 가장 빨리 달리는 문제를 한 번에 풀지 않는다.

## 2. 핵심 아이디어와 수학적 직관

일반적인 매니퓰레이터 동역학은 다음과 같이 쓸 수 있다.

\[
M(q)\ddot q+C(q,\dot q)\dot q+g(q)=\tau
\]

경로가 `q=q(s)`로 고정되면 chain rule로:

\[
\dot q=q_s\dot s,\qquad
\ddot q=q_s\ddot s+q_{ss}\dot s^2
\]

이를 동역학 식에 넣으면 각 관절 토크는 개념적으로 다음 꼴이 된다.

\[
\tau_i=a_i(s)\ddot s+b_i(s)\dot s^2+c_i(s)
\]

각 관절의 `tau_min <= tau_i <= tau_max`는 경로 상태 `(s, s_dot)`마다 허용되는 `s_ddot`의 위·아래 경계를 만든다. 그래서 원래의 고차원 관절 제어 문제가 **경로 좌표 하나의 phase plane `(s, s_dot)`에서 가능한 가속도 구간을 찾는 문제**로 축소된다.

시간을 줄이려면 가능한 동안 최대 가속을 쓰고, 목표 종료 조건을 맞추기 위해 어느 지점부터 최대 감속으로 바꿔야 한다. 직관적으로 자동차가 직선에서 가속하다가 제동 한계에 맞춰 브레이크를 밟는 것과 같다. 다만 로봇에서는 자세와 속도에 따라 각 관절 토크 제약이 바뀌므로 전환 곡선이 단순한 한 점이 아니다. 논문은 이 제한 곡선과 forward/backward integration을 이용해 가속/감속 전환을 찾는다.

## 3. 오늘 코드와 연결

오늘의 5차 다항식은 여섯 경계조건 `q0,v0,a0,qf,vf,af`를 정확히 만족하고 부드러운 기준 궤적을 만든다. 하지만 실행 시간 `T=2.6 s`는 사람이 정했다. 질량행렬, 중력, Coriolis 항, 모터 토크 한계를 보지 않으므로 그 시간이 가능한지 또는 최소인지 알 수 없다.

| 관점 | 오늘 실습 | Bobrow et al. |
|---|---|---|
| 입력 | 관절별 시작/종료 상태와 고정 `T` | 지정 경로 `q(s)`, 동역학, 토크 한계 |
| 출력 | 경계조건을 만족하는 5차 기준 상태 | 토크 한계 내 최소시간 `s(t)`와 open-loop 토크 |
| 최적성 | 없음 | 모델/가정 아래 시간최적 |
| 부드러움 | 위치·속도·가속도 연속 | 가속/토크 전환이 공격적일 수 있음 |
| 피드백 | 교육용 feed-forward+PD plant | open-loop 최적 토크 + 선형 feedback 구현 제안 |

실무 파이프라인에서는 오늘 코드와 논문을 경쟁 관계로 볼 필요가 없다. `geometric path → 동역학/제약 기반 time parameterization → spline/polynomial representation → tracking controller → independent safety monitor`처럼 계층으로 연결할 수 있다.

## 4. 실무 적용 가능성

- **산업용 pick-and-place:** 같은 충돌 회피 경로를 cycle time이 최소가 되도록 실행 시간을 줄일 수 있다.
- **CNC/용접/도장:** end-effector 경로는 공정이 정하고, 관절 토크 한계가 허용하는 속도 profile을 계산하는 구조에 맞는다.
- **현대 time parameterization의 해석 기반:** TOPP 계열 알고리즘을 공부할 때 `q(s)`로 차원을 줄이고 가속도 가능 영역을 보는 출발점이 된다.
- **offline baseline:** 최적화 기반 최신 방법의 결과가 물리적으로 말이 되는지 비교하는 기준이 된다.

## 5. 한계와 엔지니어링 주의점

1. **모델 오차:** 질량, 마찰, payload가 틀리면 토크 한계 바로 근처의 open-loop 계획은 실제로 포화될 수 있다. 여유율과 online feedback이 필요하다.
2. **지정 경로 의존:** 주어진 경로가 충돌하거나 특이점을 지나면 빠른 time parameterization이 문제를 해결하지 못한다.
3. **jerk/진동:** 시간최적 bang-bang 성격은 jerk와 구조 진동, 기어 백래시, 승차감 요구에 불리할 수 있다. 실무에서는 jerk 제한이나 smoothing을 추가한다.
4. **불연속 전환 민감도:** switching point 근처의 수치 오차와 제약 곡선 접선 조건을 견고하게 처리해야 한다.
5. **RT와 최적화는 별개:** offline에서 최적 profile을 찾았다는 사실은 online controller의 deadline, memory, communication 안전을 보증하지 않는다.
6. **기능 안전 부재:** 토크 제한 계산은 안전 인증된 감속/정지, workspace monitor, STO를 대신하지 않는다.

## 엔지니어를 위한 결론

5차 다항식은 “경계 상태를 매끄럽게 잇는 표현”이고, Bobrow 알고리즘은 “이미 정한 경로를 동역학 한계 안에서 얼마나 빨리 달릴지 정하는 방법”이다. 둘을 섞어 말하면 `T`를 작게 정한 다항식을 시간최적이라고 오해하게 된다. 실제 시스템에서는 기하 경로, 시간매개화, 추종 제어, RT 실행, 안전 감시를 서로 다른 계약과 검증 항목으로 유지하는 편이 강건하다.
