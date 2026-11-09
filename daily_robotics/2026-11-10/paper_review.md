# 논문 리뷰 — The Normal Distributions Transform: A New Approach to Laser Scan Matching

## 서지 정보와 선정 이유

- Peter Biber, Wolfgang Straßer, IROS 2003, pp. 2743–2748
- DOI: [10.1109/IROS.2003.1249285](https://doi.org/10.1109/IROS.2003.1249285)
- 공개 원문: [CiteSeerX PDF](https://citeseerx.ist.psu.edu/document?doi=1f16244ce2e78881c7c96b33796a930ce73f7972&repid=rep1&type=pdf)

최신 논문을 좇기보다 오늘은 LiDAR localization/SLAM에서 여전히 널리 쓰이는 NDT의 출발점을 골랐다. ICP처럼 명시적 point correspondence를 매 반복마다 찾는 방식과 다른 표현을 이해하면, modern 3D voxel NDT나 LiDAR odometry 구현을 읽을 때 “왜 Gaussian cell을 만드는가”가 보인다.

## 1. 해결하려는 문제

이동 로봇이 연속으로 얻은 2D laser scan 두 장을 맞추려면 상대 이동 `(x,y,yaw)`을 알아야 한다. 당시의 많은 방법은 점-점 또는 점-선 대응을 먼저 찾고 그 대응 오차를 최소화했다. 하지만 반복마다 correspondence search 비용이 들고, 가까운 점이 정말 같은 물체라는 보장도 약하다.

논문은 질문을 바꾼다.

> reference scan을 “개별 점 목록” 대신, 공간 각 구역에서 점이 나타날 확률을 설명하는 smooth distribution으로 바꾸면 current point의 적합도를 바로 계산할 수 있지 않을까?

이것이 NDT의 핵심 문제 정의다. scan matching은 localization과 SLAM의 low-level building block이며, 이 단계가 흔들리면 pose graph의 edge 자체가 잘못된다.

## 2. 핵심 아이디어와 수학적 직관

### 2.1 셀 하나를 작은 Gaussian surface model로 본다

2D 공간을 격자로 나누고 각 셀에 들어온 reference 점들의 평균 `μ`와 공분산 `Σ`를 계산한다. 평균은 그 셀의 표면 위치, 공분산의 긴 축은 표면이 뻗는 방향, 짧은 축은 표면 법선 방향의 불확실성을 나타낸다.

벽 위 점들이 있는 셀은 벽 방향으로 길고 법선 방향으로 좁은 타원형 Gaussian이 된다. current point가 벽을 따라 조금 움직이는 것은 작은 penalty지만 벽을 뚫고 법선 방향으로 움직이면 큰 penalty가 된다. Euclidean 거리 하나보다 환경 기하를 더 잘 반영한다.

### 2.2 correspondence 대신 density score를 평가한다

pose `ξ`로 current point `p_i`를 reference frame에 옮긴 `q_i(ξ)`가 어느 셀에 들어가는지 찾고, 그 셀의 Gaussian에서 likelihood를 평가한다.

\[
p(q_i) \propto \exp\left(-\frac{1}{2}(q_i-\mu)^T\Sigma^{-1}(q_i-\mu)\right)
\]

가운데 Mahalanobis 항은 단순 거리뿐 아니라 셀의 방향별 uncertainty를 반영한다. 모든 current point의 score가 커지는 pose를 찾으면 두 scan이 정렬된다. 원 논문은 이 smooth score의 도함수를 이용해 Newton 최적화를 수행한다.

### 2.3 왜 “piecewise smooth”가 중요한가

한 셀 내부에서는 Gaussian density가 미분 가능하므로 gradient/Hessian 기반 optimization을 쓸 수 있다. 명시적 nearest-neighbor correspondence를 매번 구성하지 않아도 된다는 것이 논문의 주된 장점이다. 다만 점이 셀 경계를 넘으면 사용하는 Gaussian이 바뀌므로 전역적으로 완전히 매끈한 함수는 아니다. 논문의 multiple overlapping grids는 이 불연속 영향을 줄이기 위한 중요한 장치다.

### 2.4 오늘 코드와 원 논문의 경계

오늘 코드는 원 논문의 score/Newton 전체를 복제하지 않는다. 이해와 bounded execution을 위해 다음처럼 축소했다.

- 한 개 20×20 grid만 사용한다.
- Gaussian negative log-likelihood에 대응하는 Mahalanobis 제곱합을 Gauss–Newton으로 최소화한다.
- reference 192점, scan 96점, 반복 6회로 제한한다.
- 공분산에 `λI`를 더하고 3×3 Hessian pivot과 solve failure를 telemetry로 낸다.
- 이전 pose를 local initial guess로 사용한다.

즉 “NDT 계열 point-to-distribution 교육 구현”이며, 원 논문의 실험을 재현했다는 주장이 아니다.

## 3. 실무 적용 가능성과 한계

### 적용할 만한 곳

- **LiDAR odometry/localization:** map voxel의 mean/covariance에 current scan을 맞춰 map frame pose를 갱신한다.
- **SLAM front-end:** scan-to-scan 또는 scan-to-submap 상대 pose를 구해 factor/edge 후보를 만든다.
- **초기 geometric verification:** Scan Context 같은 place recognition이 제안한 loop 후보의 상대 pose와 정합 품질을 확인한다.
- **표면 압축 표현:** raw points 대신 local Gaussian 통계로 geometry의 위치와 방향성을 표현한다.

### 한계와 제품화 시 주의점

1. **Local optimizer다.** 초기 pose가 basin 밖이면 엉뚱한 구조에 맞거나 valid cell을 충분히 찾지 못한다. wheel/IMU prediction, coarse-to-fine search, global relocalization이 필요하다.
2. **Cell size가 bias-variance를 결정한다.** 너무 크면 모서리와 서로 다른 표면을 하나의 Gaussian으로 뭉개고, 너무 작으면 셀당 점 수가 부족해 covariance가 불안정하다.
3. **기하 degeneracy가 있다.** 긴 복도처럼 평행한 벽만 보이면 복도 방향 translation이 약하게 관측된다. Hessian eigenvalue/condition, pivot, innovation을 감시해야 한다.
4. **동적 물체와 outlier에 자동 면역이 아니다.** 사람·차량이 Gaussian map이나 score를 오염시킬 수 있다. semantic/dynamic mask, robust loss, gating이 필요하다.
5. **시간·frame 오류는 optimizer가 고칠 수 없다.** scan deskew, hardware timestamp, extrinsic/TF가 틀리면 NDT는 체계적 bias를 “최적 pose”로 흡수한다.
6. **평균 실행시간은 RT 보장이 아니다.** point/voxel/iteration 상한, allocator, executor/RMW, CPU scheduling, worst-case trace가 함께 필요하다.

## 엔지니어의 결론

NDT의 가장 중요한 통찰은 “가까운 점을 찾는 문제”를 “local surface distribution에서 관측 확률을 높이는 문제”로 바꾼 것이다. 구현할 때는 score 식보다 데이터 계약과 실패 telemetry가 더 중요할 때가 많다. map/scan frame과 timestamp를 먼저 고정하고, cell occupancy·covariance conditioning·used point·Hessian pivot·iteration/time budget을 모두 외부에 보여줘야 NDT를 안전한 navigation stack의 한 구성요소로 다룰 수 있다.
