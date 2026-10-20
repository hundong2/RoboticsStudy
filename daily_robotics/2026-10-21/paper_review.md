# 논문 리뷰 — State Estimation for Legged Robots: Consistent Fusion of Leg Kinematics and IMU

## 서지 정보

- Michael Bloesch, Marco Hutter, Mark A. Hoepflinger, Stefan Leutenegger, Christian Gehring, C. David Remy, Roland Siegwart
- *Robotics: Science and Systems VIII*, 2012
- DOI: [10.15607/RSS.2012.VIII.003](https://doi.org/10.15607/RSS.2012.VIII.003)
- [RSS proceedings](https://www.roboticsproceedings.org/rss08/p03.html) · [원문 PDF](https://www.roboticsproceedings.org/rss08/p03.pdf)

## 한 문장 요약

IMU 적분만으로는 빠르게 drift하고, 다리 kinematics만으로는 접촉이 바뀔 때 기준점이 흔들리므로, **몸체 상태와 각 foothold의 월드 위치를 한 EKF state에 함께 넣고 접촉하는 발을 일시적인 랜드마크처럼 사용**한다.

## 1. 해결하려는 문제

보행 로봇의 제어기는 수백 Hz로 몸체 자세와 속도를 알아야 한다. 카메라나 LiDAR는 유용하지만 계산량, 조명, texture, latency에 영향을 받는다. 반면 거의 모든 보행 로봇에는 IMU와 joint encoder가 이미 있다.

문제는 두 센서가 서로 다른 약점을 가진다는 점이다.

- IMU의 가속도와 각속도를 적분하면 bias 때문에 위치·속도가 drift한다.
- joint encoder는 몸체와 발 사이의 상대 geometry를 주지만, 어느 발이 고정 기준인지 접촉 상태에 따라 바뀐다.
- stance 발도 실제로는 미끄러지거나 지면 compliance로 움직일 수 있다.
- 전역 위치와 yaw처럼 센서 조합으로 본질적으로 관측할 수 없는 방향을 EKF가 잘못 “확실하다”고 믿으면 consistency가 무너진다.

논문은 평평한 지면이나 특정 보행 패턴을 가정하지 않고, intermittent contact 자체를 state estimation 모델에 넣는 것을 목표로 한다.

## 2. 핵심 아이디어와 수학적 직관

### 몸체와 foothold를 같은 state에 둔다

개념적인 state는 다음처럼 볼 수 있다.

\[
x = (p, v, q, b_a, b_\omega, p_{f_1}, \ldots, p_{f_N})
\]

`p,v,q`는 몸체 위치·속도·자세, `b_a,b_ω`는 IMU bias, `p_f`는 각 발 접촉점의 월드 위치다. IMU는 몸체를 propagation하고, forward kinematics는 몸체와 발 사이 상대 위치를 측정한다.

\[
z_{f_i} \approx R(q)^T(p_{f_i}-p) + n_i
\]

직관적으로 stance 발은 잠시 월드에 박힌 landmark다. 몸체가 움직이면 encoder가 보는 발의 상대 위치가 반대로 변하므로 IMU drift를 교정할 수 있다. 발이 swing으로 들어가면 그 제약의 process noise를 키우거나 해당 contact model을 약하게 만들어 “고정 발” 가정을 해제한다.

### observability를 보존한다

외부 절대 위치/yaw 센서가 없다면 global translation과 yaw는 관측 불가능하다. 그런데 일반 EKF가 매 step 현재 추정점에서 Jacobian을 다시 만들면, 원래 시스템에 없는 정보가 수치적으로 생겨 covariance가 지나치게 작아질 수 있다.

논문의 observability-constrained EKF는 선형화된 시스템도 실제 nonlinear 시스템과 같은 unobservable subspace를 유지하도록 Jacobian을 구성한다. 엔지니어 관점에서 핵심은 **값이 그럴듯한 것과 불확실성이 정직한 것은 별개**라는 점이다. controller와 safety supervisor가 covariance를 사용한다면 consistency가 평균 오차만큼 중요하다.

### 접촉 전환은 hybrid estimation 문제다

발이 닿고 떨어질 때 유효한 측정 모델이 바뀐다. 접촉점 state를 명시적으로 두면 이 전환과 그 불확실성을 filter 안에서 표현할 수 있다. 단순히 `contact=true`일 때 몸체 속도를 0으로 덮어쓰는 것보다 모델의 출처와 covariance가 분명하다.

## 3. 실무 적용 가능성

- quadruped/humanoid의 빠른 proprioceptive odometry front-end
- visual/LiDAR odometry가 잠시 끊겼을 때 고주기 propagation과 short-term bridge
- state estimator와 whole-body controller 사이의 contact-aware velocity/pose 제공
- 각 발의 slip 가능성을 innovation과 foothold covariance로 감시
- 외부 pose sensor를 추가한 larger fusion stack의 process/measurement backbone

오늘 실습은 이 구조의 가장 작은 단면을 보여준다. `x=[z,v_z,b_a]`를 IMU로 예측하고, normal force hysteresis가 stance일 때만 `z=L cos(q)`, `v_z=-L sin(q)qdot`를 measurement로 쓴다. 100 ms encoder outlier는 2D NIS gate로 거부한다.

## 4. 한계와 제품화 체크리스트

### 논문 접근의 한계

- 발이 고정됐다는 가정이 slip, deformable terrain, foot rolling에서 깨진다.
- contact detector가 늦거나 잘못되면 틀린 kinematic constraint가 body state를 끌어간다.
- global position과 yaw drift는 외부 관측 없이는 제거할 수 없다.
- encoder zero, link geometry, IMU extrinsic/time offset 오차가 measurement bias가 된다.
- EKF consistency를 개선해도 모든 실행 시간이 bounded되거나 hard RT가 되는 것은 아니다.

### 오늘 코드가 논문보다 단순한 부분

| 원 논문 | 오늘 실습 |
|---|---|
| 3D 몸체 pose/velocity/bias와 여러 foothold | 수직 `z`, `vz`, 가속도 bias 3 states |
| 회전 manifold와 full kinematics | 한 링크 `z=L cos(q)` |
| observability-constrained linearization | 일반 fixed-size EKF |
| 접촉별 foothold uncertainty | force hysteresis + 고정 `R` |
| 실제 quadruped 검증 | 결정론적 synthetic signal |

따라서 오늘 코드의 낮은 RMSE를 실제 보행 로봇 성능으로 일반화하면 안 된다. 다음 단계는 multi-foot 3D error-state model, contact별 covariance, slip state, time-offset/extrinsic calibration, invariant/observability-aware filter, rosbag fault injection이다.

## 구현을 읽으며 답할 질문

1. `Fz <= 40 N` 첫 표본부터 constraint를 끄고 상태 전이는 세 표본 뒤 확정하는 이유는 무엇인가?
2. IMU raw acceleration에서 중력을 빼려면 왜 orientation과 frame transform이 필요한가?
3. `r^T S^-1 r` 대신 두 scalar variance를 더한 오늘 NIS 근사가 실패하는 경우는 무엇인가?
4. queue drop을 숨기지 않고 진단으로 올리는 것이 estimator tuning에 왜 중요한가?
5. global yaw와 position이 관측 불가능한데 covariance가 계속 줄어든다면 controller에 어떤 위험이 생기는가?
