# 논문 리뷰 — Monte Carlo Localization: Efficient Position Estimation for Mobile Robots

## 서지 정보와 선정 이유

- Dieter Fox, Wolfram Burgard, Frank Dellaert, Sebastian Thrun
- *Proceedings of the 16th National Conference on Artificial Intelligence (AAAI '99)*, pp. 343–349, 1999
- [CMU Robotics Institute 출판 페이지](https://publications.ri.cmu.edu/monte-carlo-localization-efficient-position-estimation-for-mobile-robots)
- [원문 PDF](https://www.cs.cmu.edu/~thrun/papers/fox.aaai99.pdf)

오늘은 “최신 논문” 대신 현재 AMCL 계열 시스템을 이해하는 데 여전히 직접 연결되는 **근간 논문**을 골랐다. 당시의 grid 기반 Markov localization은 3차원 자세 `(x,y,θ)`를 촘촘한 격자로 표현하면 계산·메모리가 커지고, 격자를 거칠게 만들면 정확도를 잃는 문제가 있었다. MCL은 확률 질량이 필요한 곳에 sample을 집중하는 관점을 제시했다. 저자들은 이전 grid 방식보다 한 자릿수(order-of-magnitude) 적은 계산량으로 정확도를 높인 실험 결과와 online sample-size adaptation을 보고했다.

## 1. 해결하려는 문제

이동 로봇의 localization은 “직전 자세 하나”를 적분하는 문제가 아니다. 실세계에서는 다음이 동시에 생긴다.

- wheel slip과 encoder 오차 때문에 dead reckoning이 계속 떠돈다.
- 복도·방처럼 비슷한 장소가 많아 관측 하나가 여러 자세와 맞을 수 있다.
- 시작 자세를 모르는 global localization에서는 belief가 넓고 다봉(multimodal)이다.
- 누군가 로봇을 옮기는 kidnapped robot 상황에서는 기존 가설이 완전히 틀릴 수 있다.

Gaussian 하나만 유지하는 Kalman 계열 필터는 평균·공분산으로 표현하기 어려운 여러 자세 가설에 약하다. 반면 전체 state grid는 해상도의 세제곱에 가까운 비용을 낸다. 논문의 질문은 “임의 모양의 자세 분포를 유지하면서 계산을 실제 가능성이 있는 영역에 집중할 수 있는가?”이다.

## 2. 핵심 아이디어와 수학적 직관

### Bayes filter를 sample로 근사한다

Bayes filter의 belief update는 motion prediction과 sensor correction으로 나뉜다.

```text
bel_bar(x_t) = ∫ p(x_t | u_t, x_t-1) bel(x_t-1) dx_t-1
bel(x_t)     = η p(z_t | x_t) bel_bar(x_t)
```

- `x_t`: 현재 로봇 자세
- `u_t`: odometry/control 입력
- `z_t`: range sensor 관측
- `η`: 전체 확률 합을 1로 만드는 정규화 상수

MCL은 적분을 닫힌 형태로 풀지 않는다. 대신 belief를 `(자세, 중요도)` particle 집합으로 근사한다.

1. 이전 particle을 odometry motion model로 이동시킨다.
2. 그 자세라면 현재 scan이 나올 가능성 `p(z_t|x_t)`을 weight로 계산한다.
3. weight에 비례해 particle을 다시 뽑는다.

직관적으로 sensor와 잘 맞지 않는 가설은 자손을 남기지 못하고, 잘 맞는 가설 주변은 particle 밀도가 높아진다. 모든 grid cell을 계속 갱신하지 않아도 확률이 큰 영역의 해상도는 자연스럽게 높아진다.

### 왜 여러 가설을 표현할 수 있는가

긴 복도에서 같은 scan이 두 위치와 맞으면 particle 무리가 두 군집으로 남을 수 있다. 로봇이 움직여 서로 다른 관측이 생기면 한 군집의 likelihood가 커지고 다른 군집이 사라진다. 단일 Gaussian이 두 봉우리 사이의 실제로 불가능한 위치를 평균으로 내는 문제를 피할 수 있다.

### Resampling은 언제나 좋은가

Resampling은 낮은 weight particle에 쓰이는 계산을 줄이지만 반복할수록 다양성을 잃는다. 모든 scan마다 무조건 하면 particle impoverishment가 빨라진다. 오늘 코드는 다음 유효 표본 수를 사용한다.

```text
N_eff = 1 / Σ_i w_i²
```

weight가 모두 `1/N`이면 `N_eff≈N`, 하나에 몰리면 `N_eff≈1`이다. 구현은 `N_eff < N/2`일 때만 systematic resampling한다. 원 논문의 online sample adaptation과 완전히 같지는 않지만 “불확실성/퇴화 정도에 따라 계산과 표본 처리를 조절한다”는 엔지니어링 방향을 보여준다.

## 3. 오늘 코드와 논문의 연결

[`src/particle_localizer.cpp`](src/particle_localizer.cpp)는 논문의 핵심 루프를 교육용으로 축소한다.

| 논문 개념 | 오늘 구현 | 의도적 단순화 |
|---|---|---|
| 자세 분포 | `std::array<Particle, 200>` | adaptive particle count 대신 고정 상한 |
| Motion model | 좌·우 encoder의 `Δs, Δθ` + Gaussian noise | 정교한 odometry error parameter 학습 생략 |
| Range likelihood | 16개 광선과 직사각형 벽의 잔차 | occupancy grid/range finder model 생략 |
| Weight 안정화 | log likelihood + max subtraction | outlier mixture model 생략 |
| Resampling | `N_eff` gate + systematic 방식 | random particle injection 생략 |
| 자세 출력 | 가중 위치/원형 yaw 평균 + covariance | 다봉 cluster별 pose 출력 생략 |

이 코드는 알고리즘 뼈대를 이해하기에는 작지만 AMCL 대체품은 아니다. 특히 네 벽만 있는 알려진 지도와 완전한 sensor-to-base extrinsic을 가정한다.

## 4. 실무 적용 가능성

MCL이 특히 유용한 곳은 다음과 같다.

- 알려진 2D map에서 운행하는 물류 AMR/서비스 로봇
- 시작 위치가 불명확하거나 재부팅 후 global relocalization이 필요한 제품
- 복도 대칭처럼 잠시 여러 pose hypothesis를 유지해야 하는 환경
- wheel odometry의 drift를 Lidar/sonar/map 관측으로 지속 보정하는 시스템

제품화할 때는 particle update 자체보다 주변 계약이 더 중요하다.

- `odom→base_link`는 연속적 local motion, `map→odom`은 global correction으로 역할을 분리한다.
- scan timestamp에 맞춰 TF를 조회하고, 움직이는 동안의 scan distortion을 처리한다.
- sensor likelihood 계산을 distance field나 beam skipping으로 가속한다.
- localization jump를 planner/controller가 어떻게 받아들일지 명시한다.
- pose 하나뿐 아니라 covariance, 최고 군집 weight, entropy, `N_eff`, update latency를 telemetry로 남긴다.

## 5. 한계와 실패 모드

### Particle 수와 차원의 저주

2D `(x,y,θ)`에서는 수백~수천 particle이 실용적일 수 있지만 6-DoF pose, calibration, bias까지 state에 넣으면 필요한 sample이 급증한다. “비모수적이니 모든 분포를 싸게 표현한다”는 뜻은 아니다.

### Kidnapped robot recovery

기존 가설이 모두 한곳에 붕괴한 뒤 로봇이 순간 이동하면, resampling만으로 전혀 다른 위치의 가설이 다시 생기지 않는다. random particle injection이나 recovery distribution이 필요하다.

### Sensor model의 과신

Gaussian sigma를 너무 작게 잡으면 작은 map 오차나 사람 같은 동적 장애물에도 weight가 한두 particle로 붕괴한다. 너무 크게 잡으면 수렴이 느리고 모호성이 오래 남는다. 실제 환경의 residual 분포와 outlier 비율로 튜닝해야 한다.

### 평균 pose의 함정

두 강한 군집이 남아 있을 때 전체 평균은 벽 속이나 두 복도 사이처럼 불가능한 자세일 수 있다. controller에 바로 넘기기 전에 dominant cluster와 모드 전환 조건을 관리해야 한다.

### 안전성과 실시간성

particle 계산 시간이 scan/particle 수에 따라 흔들릴 수 있다. deadline miss가 곧 모터 안전 문제로 이어지지 않도록 localization은 제어 loop와 분리하고, 오래된 pose를 탐지하는 watchdog과 감속/정지 정책을 둬야 한다.

## 엔지니어가 가져갈 결론

MCL의 본질은 “난수를 많이 뿌린다”가 아니라 **복잡한 belief의 계산 예산을 가능성 높은 자세 가설에 재배치한다**는 데 있다. 좋은 구현은 particle 수만 키우지 않는다. motion/sensor model을 데이터로 맞추고, 퇴화와 다봉성을 관측하며, timestamp/TF 계약을 지키고, 추정 실패를 제어 안전 상태로 연결한다.
