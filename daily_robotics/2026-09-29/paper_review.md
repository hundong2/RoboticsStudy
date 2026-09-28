# Paper Review — iSAM2: Incremental Smoothing and Mapping Using the Bayes Tree

## 논문 정보

- Michael Kaess, Hordur Johannsson, Richard Roberts, Viorela Ila, John J. Leonard, Frank Dellaert
- *The International Journal of Robotics Research*, 31(2), 216–235, 2012
- DOI: [10.1177/0278364911430419](https://doi.org/10.1177/0278364911430419)
- 저자 공개본: [Georgia Tech repository](https://repository.gatech.edu/items/b464532b-1643-4a01-bb2b-ad8690c70b4c)

이 논문은 오늘 코드에 그대로 들어간 알고리즘 설명서가 아니다. 오늘 코드는 dense fixed-lag normal equation을 매번 다시 풀고, iSAM2는 큰 sparse 그래프에서 바뀐 부분만 재계산한다. 비교 지점을 분명히 하고 읽어야 한다.

## 1. 해결하려는 문제

SLAM과 다중 센서 smoothing은 다음 비선형 최소제곱 문제로 볼 수 있다.

```text
Θ* = arg min_Θ Σ_k ||h_k(Θ_k) - z_k||²_Σk⁻¹
```

새 pose와 측정이 계속 추가되면 batch solver는 과거 전체를 다시 linearize하고 factorize한다. 그래프가 커질수록 이 방식은 online robot에서 비싸다. 초기 iSAM은 square-root information matrix를 incremental update했지만, 좋은 sparsity를 위한 variable reordering과 nonlinear relinearization을 주기적인 batch 단계에 의존했다. 즉 “평소에는 빠르지만 가끔 큰 계산 spike”가 생기며, relinearization 주기도 heuristic이었다.

iSAM2의 질문은 명확하다.

> 새 factor가 그래프의 일부만 바꿀 때, 과거 전체를 다시 풀지 않고 필요한 변수만 재정렬·재선형화할 수 있는가?

## 2. 핵심 아이디어와 수학적 직관

### Factor graph와 sparse elimination

각 측정 factor는 소수 변수에만 연결되므로 Jacobian과 information matrix는 sparse하다. 변수 제거(elimination)는 확률 분포를 조건부 분포의 곱으로 바꾸며, 선형대수 관점에서는 QR/Cholesky factorization과 대응한다. 계산량을 줄이는 핵심은 단순히 행렬 크기를 줄이는 것이 아니라 **fill-in이 적은 제거 순서**를 유지하는 것이다.

### Bayes tree

iSAM2는 제거 결과인 Bayes net의 clique들을 tree로 묶은 Bayes tree를 사용한다. 새 factor가 들어오면 그 factor에 연결된 변수의 clique와 root까지의 경로만 영향을 받는다. 영향을 받은 subtree를 떼어내고 새 factor와 함께 다시 제거한 뒤 tree에 붙인다. 멀리 떨어진 과거 clique는 그대로 재사용한다.

직관적으로 보면 “큰 sparse 행렬 전체를 다시 분해”하는 대신 “확률적 의존 구조에서 더러워진 가지를 찾아 그 가지만 다시 접는” 방법이다.

### Fluid relinearization과 incremental reordering

비선형 factor는 현재 추정점 주변에서만 정확하다. iSAM2는 변수별 선형화 오차가 threshold를 넘을 때 선택적으로 relinearize하고, affected variables의 ordering도 incremental하게 갱신한다. 이로써 주기적인 full batch step 없이 정확도와 sparsity를 유지하려 한다.

중요한 점은 “항상 일정 시간”이 아니라 “대부분의 update가 국소적이면 계산을 재사용한다”는 것이다. 큰 loop closure는 Bayes tree의 넓은 부분을 바꿀 수 있어 update cost가 커질 수 있다.

## 3. 실무 적용 가능성

### 유용한 곳

- LiDAR/visual-inertial SLAM에서 pose, landmark, bias, calibration 변수를 online smoothing할 때
- GNSS, wheel, IMU, camera처럼 rate와 noise 특성이 다른 factor를 같은 posterior에 통합할 때
- loop closure가 들어와 과거 trajectory를 다시 고쳐야 할 때
- 최신 상태뿐 아니라 과거 상태와 marginal covariance가 필요한 mapping/calibration 문제

GTSAM의 `ISAM2`와 `IncrementalFixedLagSmoother`가 대표적인 실무 구현 경로다. 제품에서는 factor 정의보다 timestamp/key lifecycle, covariance calibration, outlier 정책, graph growth/marginalization, update latency 계측이 더 자주 실패 원인이 된다.

### 오늘 실습과 연결

| 오늘 구현 | iSAM2/제품 구현 |
|---|---|
| 8개 `[x,y]` pose, 최대 16 변수 | 수천~수백만 SE(2)/SE(3), landmark, bias 변수 |
| dense normal equation을 4회 다시 풂 | Bayes tree affected clique만 재제거·재선형화 |
| oldest pose를 Schur complement로 marginalize | fixed-lag key 제거와 sparse marginal prior 관리 |
| Huber IRLS로 GNSS 이상치 완화 | robust noise model, switchable/max-mixture 등 선택 가능 |
| pivot proxy와 표본 solve 시간 | clique/update latency, fill-in, relinearization count, memory 계측 |

따라서 오늘 코드는 factor residual/Jacobian/information/marginalization을 손으로 확인하기 위한 작은 수치 실험이다. iSAM2의 Bayes tree나 sparse incremental update를 구현했다고 주장하지 않는다.

## 4. 한계와 엔지니어링 위험

1. **최악 update 시간은 작지 않을 수 있다.** 큰 loop closure, 나쁜 ordering, 광범위 relinearization은 많은 clique를 다시 계산한다. 평균 속도를 hard deadline으로 오해하면 안 된다.
2. **잘못된 covariance는 구조가 좋아도 못 고친다.** GNSS multipath나 wheel slip을 Gaussian factor로 과신하면 posterior가 정밀해 보이면서 틀릴 수 있다.
3. **강건 손실도 만능이 아니다.** Huber는 큰 residual의 영향력을 줄이지만, 지속적인 bias·잘못된 data association·집단 outlier를 자동 판별하지 않는다.
4. **Marginalization은 정보를 압축한다.** 오래된 비선형 factor를 한 선형 prior로 남기면 이후 linearization point가 움직일 때 inconsistency가 생길 수 있다.
5. **Gauge freedom과 observability를 다뤄야 한다.** global pose prior, datum, extrinsic/time offset의 관측 가능성을 명시하지 않으면 Hessian이 rank deficient해진다.
6. **메모리/실시간 경계는 별도 설계다.** GTSAM의 동적 자료구조, allocator, middleware, scheduler를 포함한 WCET는 논문 성능과 다른 문제다.

## 엔지니어 관점의 결론

iSAM2의 가장 중요한 공헌은 “SLAM을 빨리 푸는 요령”보다 sparse factorization을 확률 그래프의 국소 편집으로 이해하게 한 점이다. 이 관점 덕분에 어떤 새 factor가 어디를 다시 계산하게 하는지, relinearization과 ordering이 왜 함께 필요한지 설명할 수 있다.

오늘 코드에서 가져갈 실무 습관은 세 가지다.

1. 측정은 topic 이름이 아니라 **timestamp, frame, covariance를 포함한 factor 계약**으로 본다.
2. outlier 억제, marginalization, condition, latency를 estimator 밖에서 관찰 가능하게 만든다.
3. 평균 성능, bounded teaching kernel, hard real-time guarantee를 서로 다른 주장으로 구분한다.
