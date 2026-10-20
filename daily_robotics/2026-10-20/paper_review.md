# 논문 리뷰 — iSAM2: Incremental Smoothing and Mapping Using the Bayes Tree

## 서지 정보

- Michael Kaess, Hordur Johannsson, Richard Roberts, Viorela Ila, John J. Leonard, Frank Dellaert
- *The International Journal of Robotics Research*, 31(2), 216–235, 2012
- DOI: [10.1177/0278364911430419](https://doi.org/10.1177/0278364911430419)
- 저자 최종 원고: [MIT DSpace](https://hdl.handle.net/1721.1/78894)
- 구현 참고: [GTSAM ISAM2](https://borglab.github.io/gtsam/isam2/)

## 1. 해결하려는 문제

로봇이 움직일 때 odometry, landmark, loop closure 같은 측정은 계속 추가된다. 매 측정마다 전체 비선형 factor graph를 처음부터 batch 최적화하면 정확하지만, 지도가 커질수록 계산 낭비가 커진다. 반대로 과거 상태를 버리는 filter는 빠르지만 SLAM의 강한 비선형성과 뒤늦은 loop closure에서 일관성을 잃기 쉽다.

iSAM1은 이전 square-root information factorization을 재사용했지만, 좋은 sparsity를 위한 변수 재정렬과 비선형 재선형화를 위해 가끔 큰 batch 작업이 필요했다. 이 주기적 spike는 온라인 로봇에서 지연을 예측하기 어렵게 만든다. iSAM2가 푸는 질문은 다음과 같다.

> 새 factor가 들어올 때, 실제로 영향받은 변수와 조건부 확률만 다시 계산하면서도 smoothing의 정확도를 유지할 수 있는가?

## 2. 핵심 아이디어와 수학적 직관

### Factor graph를 Bayes tree로 본다

factor graph는 전체 확률 또는 비용을 작은 factor의 곱으로 표현한다.

\[
f(\Theta) = \prod_i f_i(\Theta_i), \qquad
\Theta^* = \arg\max_\Theta f(\Theta)
\]

Gaussian 근사에서 이 문제는 sparse linear system factorization으로 이어진다. 변수 제거는 Bayes net을 만들고, chordal 구조를 clique 단위의 **Bayes tree**로 묶을 수 있다. 핵심은 Bayes tree가 단순한 시각화가 아니라 square-root information matrix의 조건부 확률 구조를 담는 계산 자료구조라는 점이다.

### 새 factor는 tree 전체가 아니라 일부 clique만 더럽힌다

새 측정이 특정 변수에 연결되면, 그 변수와 관련된 clique부터 root 방향의 일부만 영향을 받는다. iSAM2는 그 부분을 떼어내 재선형화·재제거하고, 바뀌지 않은 subtree는 그대로 재사용한다. “이전 계산을 캐시한다”보다 더 정확히는 **확률적 조건부 구조 단위로 재사용 범위를 판별한다**.

### Fluid relinearization

비선형 최적화는 현재 추정점 주변에서 선형화한다. 모든 변수를 매번 재선형화하면 비싸고, 전혀 하지 않으면 오차가 누적된다. iSAM2는 변수별 변화량이 threshold를 넘었는지 추적하여 필요한 변수만 재선형화한다. 재정렬도 새 변수와 영향받은 부분을 중심으로 점진적으로 수행한다.

직관적으로는 다음과 같다.

1. 새 factor와 새 variable을 추가한다.
2. 추정 변화가 큰 variable을 표시한다.
3. 새 factor 또는 표시된 variable이 닿는 Bayes-tree 상단부만 다시 만든다.
4. back-substitution으로 증분 `Δ`를 구한다.
5. 현재 추정 `Θ ⊕ Δ`를 얻는다.

## 3. 실무 적용 가능성

### 잘 맞는 곳

- visual-inertial SLAM과 lidar SLAM backend
- GPS가 늦게 오거나 loop closure가 과거 pose에 연결되는 multi-sensor fusion
- online calibration처럼 pose 외 파라미터도 graph variable로 함께 추정하는 문제
- full trajectory와 marginal covariance가 필요한 mapping/inspection 시스템

GTSAM의 `ISAM2`가 대표적인 구현이며, `Pose3`, IMU preintegration, robust noise model과 조합할 수 있다. ROS 2에서는 frontend callback에서 바로 큰 graph update를 수행하기보다, timestamp·frame·covariance를 검증한 factor queue와 backend optimizer 실행 경계를 분리하는 편이 안전하다.

### 오늘 코드와 연결

오늘의 실습도 늦게 도착한 absolute factor를 `header.stamp`로 창 안의 과거 state에 연결한 뒤 최신 자세까지 다시 푼다. 이 점은 incremental smoothing의 동기를 직접 보여 준다. 또한 window, dimension, iteration을 고정하여 계산 예산을 외부 진단으로 노출한다.

그러나 오늘 코드는 학습용 dense solver다.

| 항목 | 오늘 실습 | iSAM2 |
|---|---|---|
| 자료구조 | 8-pose 고정 배열과 dense `H` | sparse Bayes tree/cliques |
| 재계산 | 창 전체 3회 | 영향받은 tree 부분 |
| 재선형화 | 단순 local tangent, 고정 횟수 | threshold 기반 fluid relinearization |
| 재정렬 | 없음 | incremental variable ordering |
| marginalization | 첫 state Gaussian prior 근사 | factorization에 근거한 확률 구조 |
| 회전 | Euler 작은 각도 근사 | manifold retract/local coordinates |

## 4. 한계와 엔지니어링 주의점

1. **평균 계산량이 낮아도 최악 지연이 고정되지는 않는다.** 큰 loop closure는 Bayes tree의 넓은 부분을 건드릴 수 있다. 논문의 online 효율을 hard-RT 보장으로 읽으면 안 된다.
2. **ordering이 성능을 좌우한다.** 수학적으로 같은 factor graph도 제거 순서에 따라 fill-in과 clique 크기가 크게 달라진다.
3. **선형화점과 threshold는 정확도-성능 trade-off다.** 너무 느슨하면 비선형 오차, 너무 엄격하면 잦은 재선형화가 생긴다.
4. **outlier는 별도 문제다.** 잘못된 loop closure가 들어오면 빠르게 잘못된 답을 만들 수 있다. robust loss, switchable constraints, geometric verification이 필요하다.
5. **fixed lag는 정보 손실 정책이다.** 오래된 변수를 marginalize하면 dense prior 또는 consistency 문제가 생길 수 있다.
6. **ROS 메시지 공분산이 곧 factor noise는 아니다.** 좌표계 변환, sensor correlation, frontend가 이미 사용한 정보의 중복을 고려하지 않으면 double counting한다.

## 5. 주니어 엔지니어가 가져갈 질문

- 늦은 측정을 callback 도착시각으로 덮어쓰면 어떤 state에 factor가 잘못 연결되는가?
- `PoseWithCovariance`의 orientation covariance는 Euler 순서인데, manifold optimizer 내부 tangent covariance와 언제 같지 않은가?
- 최대 8 poses를 보존할 때 3-tick 지연은 받지만 10-tick 지연은 어떻게 처리할 것인가?
- 평균 update time 외에 affected clique 수, relinearized variable 수, maximum latency를 왜 진단해야 하는가?
- loop closure outlier가 RT deadline 안에 들어왔다는 사실이 안전성과 정확도를 보장하는가?

## 한 문장 결론

iSAM2의 본질은 “factor를 하나 더 넣는다”가 아니라, **새 정보가 확률 구조의 어디를 바꾸는지 Bayes tree로 찾아 필요한 계산만 다시 하는 것**이며, 제품에서는 이 알고리즘적 효율과 별개로 입력 계약·outlier·메모리·최악 지연을 명시적으로 관리해야 한다.
