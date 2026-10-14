# 논문 리뷰 — Galilean State Estimation for Inertial Navigation Systems with Unknown Time Delay

## 서지 정보

- Giulio Delama, Martin Scheiber, Yixiao Ge, Tarek Hamel, Stephan Weiss, Robert Mahony
- arXiv:2605.13266 [cs.RO], 2026-05-13 제출
- [초록/원문](https://arxiv.org/abs/2605.13266) · [HTML](https://arxiv.org/html/2605.13266v1)

## 1. 해결하려는 문제

IMU는 수백 Hz로 빠르게 들어오지만 GNSS 위치는 느리고, 수신기 내부 적분·통신·스레드 스케줄링 때문에 실제 측정 시점보다 늦게 도착한다. 논문이 제시한 상용 GNSS 지연 범위는 대략 50–300 ms다. 현재 자세·속도·위치에 과거 GNSS를 그대로 보정하면 차량이 그동안 이동한 양을 측정 오차로 오인한다.

일반적인 실무 해법은 지연을 오프라인 calibration하거나, IMU buffer를 되감아 과거 상태를 보정한 뒤 다시 적분하는 것이다. 그러나 지연값을 모르면 calibration이 필요하고, 지연을 평범한 EKF의 독립 scalar state로 넣으면 공간 상태와 시간 상태의 결합 구조를 선형화가 잘 보존하지 못해 bias와 inconsistency가 생길 수 있다. 이 논문은 **navigation state와 미지의 시간 지연을 온라인에서 일관되게 함께 추정**하려 한다.

## 2. 핵심 아이디어와 수학적 직관

### 공간과 시간을 한 변환으로 묶는다

보통 강체 자세는 `SE(3)`의 회전과 위치로 표현한다. 논문은 여기에 속도와 시간 offset까지 넣은 5×5 Galilean group 원소를 사용한다.

\[
F = \begin{bmatrix}
R & v & p\\
0 & 1 & \delta\\
0 & 0 & 1
\end{bmatrix}
\]

여기서 \(R\)은 회전, \(v\)는 속도, \(p\)는 위치, \(\delta\)는 시간 offset이다. 중요한 직관은 시간 지연이 위치에 미치는 영향이 속도와 분리된 우연한 항이 아니라는 점이다. 같은 물체의 `t-δ` frame과 `t` frame 사이 변환 안에서 공간 변화와 시간 변화가 함께 움직인다.

### 지연 측정을 현재 상태에 연결한다

GNSS는 \(p(t-\delta)\)를 측정한다. 논문은 buffered IMU를 preintegration한 \(\Upsilon(\delta)\)를 사용해 현재 Galilean state에서 과거 위치를 예측한다.

\[
h(\xi)=\mathcal{P}\left(F(t)\,\Upsilon(\delta)^{-1}\bar y_0\right),
\qquad r=y_m-h(\hat\xi)
\]

즉, residual은 “늦게 온 위치를 현재 위치와 빼기”가 아니라 “추정된 시간 지연만큼 되돌린 예측 위치와 빼기”다. EqF는 이 group symmetry 위에서 오차를 정의하고 선형화한다. 그 결과 시간 지연과 자세·속도·위치 오차의 구조적 coupling이 covariance와 gain에 자연스럽게 들어간다.

### consistency도 정확도만큼 본다

논문은 위치 RMSE뿐 아니라 NEES를 사용한다. 오차가 작아 보여도 covariance가 지나치게 작으면 필터는 자신감을 과대평가한 inconsistent 상태다. 실제 fixed-wing UAV 세 flight에서 90/120 ms 지연을 온라인으로 추정했고, simulation에서는 100–500 ms 지연에 대해 50회 Monte Carlo를 수행했다. 저자 결과에서 EqF는 모든 지연에서 수렴했지만 비교 EKF는 지연이 커질수록 oscillation과 inconsistency가 증가하고 500 ms에서 발산했다.

## 3. 실무 적용 가능성

- GNSS–IMU, camera–IMU, networked multi-robot처럼 지연을 완전히 고정값으로 보기 어려운 시스템에서 시간 offset을 상태 추정 문제 안으로 가져오는 설계 근거가 된다.
- 단순히 `delay` scalar를 EKF state 끝에 붙이는 것보다, 상태가 놓인 manifold와 symmetry에 맞는 error 정의가 consistency에 중요하다는 점을 보여 준다.
- 충분한 운동 excitation이 있어야 지연을 관측할 수 있다. 논문도 정지나 일정 속도 직선 운동은 지연 관측성이 퇴화한다고 지적한다. Calibration maneuver 설계가 filter 수식만큼 중요하다.
- GNSS time-of-validity, IMU hardware timestamp, clock synchronization이 불명확하면 좋은 filter도 잘못된 데이터 계약을 구할 수 없다. 센서 driver와 clock architecture를 먼저 감사해야 한다.

## 4. 한계와 엔지니어링 주의점

- 실험은 fixed-wing UAV 두 종류의 세 flight, 약 2–3분 구간이다. 다양한 GNSS multipath, drop, urban canyon, 장시간 clock drift를 모두 대표하지 않는다.
- 지연 reference는 전체 dataset offline optimization으로 구했다. 이것은 정밀한 외부 ground truth clock과 동일하지 않으므로 delay error 해석에 주의해야 한다.
- EqF의 20차 covariance, Lie group exp/log, bias-corrected IMU preintegration은 오늘의 2-state filter보다 구현·검증 비용이 훨씬 크다.
- 평균 정확도와 Monte Carlo consistency는 hard real-time deadline, buffer overflow 안전성, 메모리 상한을 증명하지 않는다.
- 시간 지연과 IMU bias/extrinsic이 서로 보상하는 잘못된 방향으로 수렴할 수 있으므로 observability, initialization, innovation/NEES, parameter bound를 함께 감시해야 한다.

## 오늘 코드와 논문의 경계

오늘 코드는 `Header.stamp`가 정확하고 지연 시각을 이미 안다는 가정 아래 1D Kalman state를 과거로 되감아 재전파한다. 논문의 핵심인 **미지 시간 지연의 온라인 추정**, 3D INS, Galilean EqF, IMU preintegration은 구현하지 않는다. 대신 제품 구현 전에 반드시 이해해야 할 세 가지—측정시각 계약, historical correction, bounded buffer—를 독립적으로 검증한다.

## 한 줄 결론

지연은 단순한 통신 문제가 아니라 상태의 시공간 geometry 문제다. 현장에서는 먼저 stamp와 buffer 계약을 고정하고, 지연이 미지수일 때는 운동 관측성과 filter consistency까지 포함해 설계해야 한다.
