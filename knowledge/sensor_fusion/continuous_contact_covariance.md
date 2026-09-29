# 연속 접촉 공분산

## 왜 binary contact가 부족한가

접촉은 AIR/STABLE 두 상태만 있는 것이 아니다. 부분 접촉, 모서리 회전, 방향성 slip, compliant surface가 있다. `contact=true`는 이 차이를 숨기고, `false`는 남아 있는 유용한 제약을 버린다.

접촉점 속도를 `w_C ~ N(0, Sigma_C)`로 두면 `Sigma_C`가 작은 방향은 정지 제약을 강하게, 큰 방향은 약하게 적용할 수 있다. 3D에서는 `Sigma_C`의 eigenvector/eigenvalue가 “어느 방향으로 얼마나 움직일 수 있는가”를 표현한다.

## 유효 covariance 조건

covariance는 대칭 positive-semidefinite여야 한다. 학습 모델이 lower-triangular `L`을 출력하고 `Sigma=L L^T`로 구성하면 이 조건을 구조적으로 만족한다. 수치적으로는 diagonal floor와 maximum cap도 두어 filter가 singular하거나 완전히 무시되는 것을 막는다.

## 실무 체크리스트

- covariance 값뿐 아니라 stamp, frame, 단위, calibration version을 계약에 포함한다.
- normalized innovation squared와 consistency metric으로 calibration을 평가한다.
- OOD에서 과신할 수 있으므로 독립 contact/slip guard를 둔다.
- covariance module의 평균 latency를 WCET로 부르지 않는다.

2026-09-30 실습은 이 원리의 scalar 축약판을 `daily_robotics/2026-09-30`에 구현한다.

참고: [CoCo-InEKF (RSS 2026)](https://arxiv.org/abs/2605.15122)
