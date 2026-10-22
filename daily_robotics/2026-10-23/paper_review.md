# 논문 리뷰 — Learned Uncertainty Calibration for Visual Inertial Localization

## 서지 정보

- Stephanie Tsuei, Stefano Soatto, Paulo Tabuada, Mark B. Milam
- IEEE International Conference on Robotics and Automation (ICRA), 2021
- DOI: [10.1109/ICRA48506.2021.9561179](https://doi.org/10.1109/ICRA48506.2021.9561179)
- 공개 원문: [arXiv:2110.02136](https://arxiv.org/abs/2110.02136)

## 1. 해결하려는 문제

EKF는 상태 평균 `x_hat`과 함께 covariance `P_hat`도 출력한다. 현장에서는 궤적 오차가 작다는 이유로 `P_hat`까지 신뢰하기 쉽지만, nonlinear model의 선형화 오차, 실제와 다른 Gaussian/noise 가정, 고정 Q/R 때문에 평균은 괜찮아도 covariance는 심하게 과신될 수 있다. 이때 downstream planner가 “3-sigma 안이니 안전하다”고 판단하면 작은 RMSE 뒤에 큰 tail risk가 숨는다.

논문의 질문은 단순하다. **EKF가 반복해서 만드는 covariance 오차에 체계적인 패턴이 있다면, 추정 covariance와 상태로부터 더 현실적인 covariance를 학습할 수 있는가?** 저자들은 2D localization simulation과 실제 TUM Visual-Inertial 데이터/XIVO에서 이를 검증한다.

## 2. 핵심 아이디어와 수학적 직관

### Calibration을 무엇으로 재는가

정답 상태 `x`가 있을 때 estimation error `e=x-x_hat`의 Mahalanobis 거리 제곱은

```text
NEES = e^T P_hat^(-1) e
```

이며 n차원 Gaussian 가정이 맞고 covariance가 calibrated라면 chi-square(n) 분포를 따라야 한다. 값이 지속적으로 너무 크면 `P_hat`이 실제 오차보다 작아 과신한다는 뜻이다. 저자들은 반복 Monte Carlo가 어려운 실세계 VIO에서 시간 구간의 error statistics가 population statistics를 대신한다는 ergodicity 가정으로 pseudo ground-truth covariance를 만든다.

### 네 단계의 보정 map

논문은 단순한 것부터 복잡한 것까지 비교한다.

1. `P_cal=s*P_hat`: 전체 covariance에 양의 scalar 하나를 곱한다.
2. `P_cal=A*P_hat*A^T`: 고정 선형 변환으로 축 간 scale/correlation을 바꾼다.
3. `Q=phi(P_hat), P_cal=Q*Q^T`: covariance만 입력하는 feed-forward neural network다.
4. `Q=phi(x_hat,P_hat), P_cal=Q*Q^T`: 상태까지 조건으로 준다.

`Q*Q^T` 형태는 network 출력이 임의 값이어도 결과 covariance를 positive semidefinite로 만드는 장치다. 실험에서는 neural map이 단순 scalar/matrix map보다 chi-square calibration을 크게 개선했고, 상태를 추가한 모델의 이득은 제한적이었다. VIO test에서 논문이 보고한 divergence 감소는 covariance-only NN 97.8%, state-dependent NN 105.6%였다. 100%를 넘는 값은 해당 empirical metric에서 pseudo-ground-truth 기준보다 더 낮은 divergence가 나왔다는 뜻이지, 모든 환경에서 “완벽 이상의 보정”을 보장한다는 뜻은 아니다.

## 3. 엔지니어 관점의 적용 가능성

### 유용한 지점

- 기존 VIO/EKF를 전면 교체하지 않고 covariance output 뒤에 post-hoc calibration layer를 붙일 수 있다.
- `P_hat` 자체가 miscalibration pattern을 많이 담는다는 결과는 runtime feature와 network 크기를 줄일 가능성을 보여 준다.
- planner의 chance constraint, sensor fusion weight, fault detector threshold가 covariance를 소비한다면 평균 궤적 정확도와 별도로 calibration test를 배포 gate에 넣어야 한다.
- positive-semidefinite parameterization, chi-square/coverage evaluation은 learned uncertainty 시스템의 기본 설계 패턴으로 재사용할 수 있다.

### 한계와 위험

- 학습과 test가 같은 sensor 및 유사 환경에서 수집되었다. 조명, vibration, lens contamination, firmware가 바뀐 OOD 환경까지 calibration이 일반화된다는 증거는 아니다.
- pseudo ground-truth covariance는 error의 느린 변화/ergodicity와 근사 independence에 의존한다. 장시간 bias나 비정상 motion에서는 깨질 수 있다.
- post-hoc covariance가 좋아져도 state mean의 bias는 고쳐 주지 않는다.
- learned covariance를 filter update 안으로 넣으면 estimate가 network 입력을 바꾸고 다시 covariance가 gain을 바꾸는 closed loop가 된다. 안정성과 bounded latency를 별도로 분석해야 한다.
- 평균 calibration score만 좋게 만드는 과정이 rare catastrophic tail을 가릴 수 있으므로 OOD detector, residual gate, fallback sensor가 필요하다.

## 오늘 코드와 논문의 경계

오늘의 [`adaptive_covariance_filter.cpp`](src/adaptive_covariance_filter.cpp)는 논문 구현의 복제가 아니다.

| 항목 | 논문 | 오늘 실습 |
|---|---|---|
| 대상 | 9D VIO localization covariance | 1-DoF `q,q_dot` Kalman filter |
| 학습 | 여러 training sequence의 supervised offline map | in-domain NIS를 clipped EWMA로 추적 |
| map | scalar, matrix, 두 종류 feed-forward NN | 양의 scalar `R_cal=s*R_model` |
| 검증 | chi-square divergence와 sigma coverage | exact-sequence RMSE, rejection, bound audit |
| OOD 대응 | 주요 초점 아님 | 명시적 model-side OOD gate + NIS gate |
| 안전 연결 | localization covariance 평가 | torque rate/power/energy supervisor 입력 |

따라서 오늘 코드는 “학습된 covariance도 calibration과 OOD 분리가 필요하다”는 직관을 작은 bounded pipeline으로 연습하는 재현 가능한 뼈대다. 실제 deployment에서는 offline calibration dataset, hold-out environment, sensor versioning, coverage/NEES report, runtime fallback을 갖춰야 한다.

## 토론 질문

1. NIS는 truth 없이 계산할 수 있지만 process-model 오류와 measurement 오류를 구분할 수 있는가?
2. calibration scale이 커질 때 learned sensor를 덜 믿는 것과 OOD로 완전히 거부하는 것의 기준은 무엇이어야 하는가?
3. covariance map을 filter loop 안에 넣을 때 stability를 어떤 실험과 이론으로 검증할 것인가?
4. 잘 calibrated된 평균 confidence interval이 safety-critical tail risk를 충분히 설명하는가?
