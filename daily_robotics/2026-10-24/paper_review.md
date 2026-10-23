# 논문 리뷰 — IMU Preintegration on Manifold for Efficient Visual-Inertial Maximum-a-Posteriori Estimation

## 서지 정보

- Christian Forster, Luca Carlone, Frank Dellaert, Davide Scaramuzza
- *Robotics: Science and Systems XI (RSS 2015)*, Rome, Italy
- DOI: [10.15607/RSS.2015.XI.006](https://doi.org/10.15607/RSS.2015.XI.006)
- 공식 proceedings: [roboticsproceedings.org/rss11/p06.html](https://www.roboticsproceedings.org/rss11/p06.html)
- 확장 원고: [arXiv:1512.02363](https://arxiv.org/abs/1512.02363)

## 1. 해결하려는 문제

카메라는 보통 수십 Hz, IMU는 수백~수천 Hz로 들어온다. keyframe pose를 optimization variable로 두고 IMU 한 샘플마다 factor와 state를 추가하면 graph가 너무 빨리 커진다. 반대로 두 keyframe 사이의 IMU를 한 번 단순 적분해 world-frame 상대 이동으로 고정하면 optimizer가 과거 자세나 bias를 바꿀 때 raw IMU 전체를 다시 적분해야 한다.

회전은 유클리드 벡터가 아니라 `SO(3)` manifold 위에 있다는 점도 문제다. Euler angle을 평범하게 더하면 큰 회전과 특이점에서 geometry가 깨지고, covariance와 Jacobian도 잘못 전파될 수 있다.

논문의 질문은 다음과 같다.

> 수백 개의 IMU 샘플을 keyframe 두 개를 잇는 하나의 factor로 압축하면서, 회전 manifold·불확실성·나중의 bias 수정을 올바르게 보존할 수 있는가?

## 2. 핵심 아이디어와 수학적 직관

### 세계 자세와 분리된 local-frame 적분

두 keyframe `i,j` 사이의 운동은 대략 다음 inertial dynamics를 따른다.

\[
R_j = R_i\Delta R_{ij},\qquad
v_j = v_i + g\Delta t + R_i\Delta v_{ij}
\]

\[
p_j = p_i + v_i\Delta t + \frac{1}{2}g\Delta t^2 + R_i\Delta p_{ij}
\]

여기서 `ΔR, Δv, Δp`를 world가 아니라 keyframe `i`의 local frame에 적분한다. optimizer가 `R_i`를 다시 추정해도 preintegrated measurement 자체는 유지되고 마지막에 `R_i`만 곱한다. 이것이 계산 절감의 본질이다.

### 회전은 exponential map으로 누적한다

각속도 측정 `ω̃ = ω + b_g + noise`에서 bias를 뺀 뒤,

\[
\Delta R_{i,k+1}=\Delta R_{ik}\operatorname{Exp}((\tilde\omega_k-b_g)\Delta t)
\]

처럼 `SO(3)` exponential map으로 누적한다. `Exp`는 3차원 회전벡터를 회전행렬 manifold로 보낸다. 따라서 rotation matrix의 orthogonality를 유지하면서 작은 tangent perturbation으로 Jacobian과 covariance를 전파할 수 있다.

### bias가 바뀌어도 raw IMU를 다시 읽지 않는다

preintegration은 특정 bias 선형화점 `b̄`에서 계산된다. optimization 중 bias가 `b̄+δb`로 바뀌면 저장한 Jacobian으로 1차 보정한다.

\[
\Delta v(b) \approx \Delta v(\bar b) + J_{v,b}\delta b
\]

회전과 위치도 같은 방식이다. bias 변화가 너무 크면 다시 preintegrate해야 하지만, 보통의 incremental update에서는 이 1차 보정으로 raw high-rate stream 재처리를 피한다.

### factor graph 안에서 visual과 inertial을 함께 푼다

preintegrated factor는 두 keyframe의 pose, velocity, bias를 연결한다. visual landmark factor는 pose와 feature를 연결한다. 이들을 같은 MAP 문제에 넣으면 filter처럼 한 번 버린 선형화점에만 의존하지 않고 최근 state를 재선형화할 수 있다.

논문은 visual landmark를 구조 없는(structureless) 형태로 제거해 landmark state 증가도 줄인다. 즉 preintegration 하나만의 논문이 아니라, optimization 기반 VIO가 실시간으로 돌아가도록 state와 measurement를 압축하는 설계다.

## 3. 실무 적용 가능성

### 특히 유용한 곳

- 드론·AR/VR·보행 로봇처럼 camera와 IMU rate 차이가 큰 시스템
- keyframe 기반 VIO/SLAM backend
- 늦은 visual correction으로 최근 trajectory를 다시 조정해야 하는 시스템
- bias와 calibration을 state로 두고 online 추정하는 estimator

GTSAM의 `PreintegratedImuMeasurements`와 `ImuFactor` 계열이 대표적인 제품화 출발점이다. ROS 2에서는 IMU callback마다 graph solve를 하지 말고, frontend queue에서 keyframe 구간을 닫아 backend에 factor를 넘기는 것이 자연스럽다.

### 오늘 코드와 직접 연결되는 부분

| 논문 | 오늘 구현 |
|---|---|
| `SO(3)` local-frame preintegration | planar `SO(2)` local-frame `Δθ,Δv,Δp` |
| analytic bias Jacobian | 고정 epsilon의 병렬 preintegration finite difference |
| propagated preintegration covariance | 고정 diagonal teaching weight |
| pose·velocity·gyro/accel bias | `[x,y,vx,vy,yaw,gyro_bias]` |
| incremental factor graph | 6-state dense fixed-lag graph |
| full visual factors | noisy absolute planar visual pose factor |

따라서 오늘 코드의 목적은 논문 재현이 아니라 **데이터 경계와 수식의 구조를 손으로 확인하는 것**이다.

## 4. 한계와 엔지니어링 위험

### Preintegration이 자동으로 RT를 보장하지 않는다

샘플 수를 줄여 평균 계산량을 낮추지만, graph ordering, relinearization 범위, loop closure, marginalization fill-in에 따라 update latency tail은 커질 수 있다. deadline 시스템에서는 queue 상한, update budget, drop/fallback 정책, tracing을 따로 설계해야 한다.

### 시간 동기와 extrinsic이 틀리면 좋은 수학도 실패한다

camera–IMU time offset, rolling shutter, `T_camera_imu`, scale, gravity 방향이 틀리면 bias가 그 오차를 대신 흡수하거나 trajectory가 일관되게 휘어진다. online calibration을 넣으면 state가 늘고 observability 조건도 더 까다로워진다.

### 강한 동작과 진동

저가 IMU의 saturation, vibration aliasing, 온도 bias, axis misalignment는 백색 Gaussian noise 가정에서 벗어난다. anti-alias filter, timestamp hardware sync, Allan variance 기반 noise identification, temperature calibration이 필요하다.

### Marginalization inconsistency

fixed-lag smoother는 오래된 state를 선형화한 뒤 prior로 굳힌다. 이후 남은 state가 크게 이동해도 그 과거 factor를 원래 nonlinear form으로 되돌릴 수 없다. gauge freedom과 unobservable direction을 잘못 고정하면 covariance가 과신될 수 있다. FEJ, null-space 처리, 관측성 분석이 필요한 이유다.

### Visual failure

texture 부족, motion blur, 반복 무늬, illumination 변화에서 visual factor가 끊기거나 outlier가 된다. preintegration은 짧은 공백을 연결하지만 IMU만으로 위치 drift를 막을 수 없다. frontend quality score, robust loss, relocalization, fail-safe가 필요하다.

## 5. 제품 코드로 확장하는 순서

1. IMU/camera hardware timestamp, frame, 단위, covariance 계약을 bag replay로 검증한다.
2. 3D `SO(3)` exponential/log map과 analytic bias Jacobian을 검증된 라이브러리로 구현한다.
3. preintegrated covariance, gravity, accelerometer bias, extrinsic을 state/factor에 포함한다.
4. fixed-lag marginalization의 gauge/consistency를 Monte Carlo NEES/NIS로 확인한다.
5. visual outlier, IMU saturation, delayed frame, queue overflow fault injection을 넣는다.
6. p50/p95/p99/max update latency와 deadline miss를 실제 target CPU/RMW에서 측정한다.
7. estimator 출력과 별도로 안전 supervisor가 stale/uncertain state에서 감속·정지하도록 한다.

## 6. 엔지니어가 기억할 한 문장

> IMU preintegration은 “고속 IMU를 대충 누적”하는 기법이 아니라, keyframe local manifold에서 측정을 압축해 optimizer의 재선형화와 bias 수정을 싸게 만드는 factor 설계다.
