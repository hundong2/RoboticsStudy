# 논문 리뷰 — 특이점에서도 멈추지 않는 역기구학

## 선정 논문

- Yoshihiko Nakamura, Hideo Hanafusa, **“Inverse Kinematic Solutions With Singularity Robustness for Robot Manipulator Control”**
- *Journal of Dynamic Systems, Measurement, and Control*, Vol. 108, No. 3, pp. 163–171, 1986
- DOI: [10.1115/1.3143764](https://doi.org/10.1115/1.3143764)
- 서지·초록: [CiNii Research](https://cir.nii.ac.jp/crid/1361137046123304448)

이 논문은 최신 논문 대신 오늘의 Jacobian IK를 떠받치는 **근간 논문**으로 골랐다. 관절형 로봇에 특이점이 본질적으로 존재하며, 역행렬·의사역행렬 해가 실현 불가능해지는 구간에서 feasible한 근사 운동을 만드는 SR-inverse를 제안한 작업이다.

## 1. 해결하려는 문제

말단 속도와 관절 속도는 국소적으로 다음 관계를 가진다.

\[
\dot{x}=J(q)\dot{q}
\]

정방 Jacobian이면 흔히 `q_dot = J^-1 x_dot`를 생각하고, 비정방이면 Moore–Penrose 의사역행렬 `J^+`를 쓴다. 하지만 팔이 완전히 펴지거나 접히는 자세처럼 Jacobian의 어떤 특이값 `sigma_i`가 0에 가까워지면 `1/sigma_i`가 매우 커진다. 작은 Cartesian 요청이나 센서 노이즈가 거대한 관절 속도로 증폭되어 actuator limit, tracking loss, 진동을 일으킬 수 있다. 정확한 Cartesian 속도를 고집한 해가 물리적으로 실행 불가능한 셈이다.

논문의 핵심 질문은 “요청을 수학적으로 정확히 역변환할 수 있는가?”가 아니라 **“현재 로봇이 실행할 수 있는 가장 가까운 운동은 무엇인가?”**다.

## 2. 핵심 아이디어와 수학적 직관

오늘 코드에서 사용하는 대표적인 singularity-robust 형태는 Damped Least Squares다.

\[
\dot{q}=J^T(JJ^T+\lambda^2I)^{-1}\dot{x}_d
\]

이는 다음 정규화 최소제곱과 같다.

\[
\dot{q}^*=\arg\min_{\dot{q}}
\left(\|J\dot{q}-\dot{x}_d\|^2+\lambda^2\|\dot{q}\|^2\right)
\]

첫 항은 말단 명령 오차, 둘째 항은 과도한 관절 속도 비용이다. `lambda=0`이면 가능한 구간에서 의사역행렬에 가까워지고, `lambda`를 키우면 Cartesian 정확도를 일부 양보하는 대신 joint velocity가 작고 안정적인 해를 택한다.

SVD 관점이 가장 직관적이다. 의사역행렬의 작은 특이값 방향 gain `1/sigma_i`를 SR/DLS는 다음처럼 바꾼다.

\[
\frac{1}{\sigma_i}
\quad\longrightarrow\quad
\frac{\sigma_i}{\sigma_i^2+\lambda^2}
\]

`sigma_i→0`일 때 오른쪽 gain도 0으로 내려간다. 즉 로봇이 현재 만들 수 없는 Cartesian 방향을 억지로 따라가려 하지 않는다. 논문은 이러한 SR-inverse를 일반 역·의사역과 비교하고, 요청 motion의 feasibility를 고려해 특이점 근방에서도 가까운 근사 운동을 구하는 관점을 정립했다.

## 3. 실무 적용 가능성

- `ros2_control` controller의 `update()`에서 Jacobian과 작은 선형계를 풀어 Cartesian servo를 만들 수 있다.
- Cartesian jog, teleoperation, visual servoing, welding/inspection path tracking에서 특이점 근처의 velocity spike를 줄인다.
- SVD를 매번 수행하지 않아도 오늘의 2×2 예제처럼 `JJ^T + lambda²I`를 작은 닫힌식으로 풀면 계산량 상한을 설명하기 쉽다.
- manipulability, 최소 특이값, 예측 joint-speed 중 하나로 `lambda`를 조절하면 정상 영역의 정확도와 특이점 영역의 안정성을 절충할 수 있다.
- 명령 속도·관절 위치 제한, watchdog, collision constraint와 결합하면 제품 controller의 안전 계층 일부가 된다.

## 4. 한계와 제품화 시 주의점

- **근사 오차:** damping은 Cartesian 명령을 정확히 만족한다는 보장을 버린다. 상위 planner가 tracking error를 허용해야 한다.
- **lambda tuning:** 너무 작으면 spike가 남고, 너무 크면 정상 영역도 둔해진다. 단일 threshold는 모든 payload·속도·자세에 맞지 않는다.
- **국소 해법:** Jacobian은 현재 자세의 미분 모델이다. 큰 목표 이동에는 작은 step 반복, line search, trajectory generation이 필요하다.
- **동역학 부재:** kinematic feasibility가 torque, friction, flexible joint, payload feasibility를 뜻하지 않는다.
- **joint limit/collision:** SR-inverse만으로 self-collision, joint-limit avoidance, obstacle avoidance를 해결하지 않는다. constrained QP나 null-space objective가 필요하다.
- **RT 보장과 별개:** 행렬식이 작고 allocation-free여도 OS scheduling, hardware bus, DDS, page fault까지 검증하지 않으면 hard RT가 아니다.

## 5. 오늘 코드와의 연결

[`src/dls_ik_controller.cpp`](src/dls_ik_controller.cpp)은 2R Jacobian을 직접 만들고 `Jᵀ(JJᵀ+lambda²I)⁻¹`를 2×2 닫힌식으로 계산한다. `|det(J)|=|l1·l2·sin(q2)|`가 작아지면 `lambda`를 키우며, Cartesian 속도와 joint 속도를 모두 제한한다.

이 구현은 논문의 실험을 복제한 것이 아니라 **SR-inverse의 핵심 직관을 학습 가능한 최소 시스템으로 옮긴 축소판**이다. 논문 수준으로 비교하려면 동일 trajectory에서 inverse, pseudoinverse, fixed damping, adaptive damping의 tracking error·최대 joint speed·연산시간을 함께 측정해야 한다.

## 엔지니어의 한 줄 결론

특이점 강건 IK는 “정확한 해가 없을 때 실패하지 않는 마법”이 아니라, **실행 불가능한 Cartesian 정확도를 의도적으로 포기하고 관절 운동의 실행 가능성을 지키는 설계된 타협**이다.
