# 논문 리뷰 — Random Sample Consensus (RANSAC)

## 논문 정보와 선정 이유

- Martin A. Fischler, Robert C. Bolles
- “Random Sample Consensus: A Paradigm for Model Fitting with Applications to Image Analysis and Automated Cartography”
- *Communications of the ACM*, 24(6), 381–395, 1981
- DOI: [10.1145/358669.358692](https://doi.org/10.1145/358669.358692)
- [원문 PDF 사본](https://graphics.stanford.edu/courses/cs233-21-spring/ReferencedPapers/ransac.pdf)

오늘은 최신 논문 대신 로봇 비전의 거의 모든 geometric front-end에 남아 있는 **근간 논문**을 골랐다. stereo/visual odometry에서 descriptor mismatch와 dynamic object는 작은 Gaussian noise가 아니라 완전히 틀린 대응점이다. RANSAC은 이 “gross error가 섞인 model fitting”을 정면으로 다룬다.

## 1. 해결하려는 문제

고전적인 least squares는 모든 관측이 같은 model에서 왔고 오차가 비교적 작다는 전제에 강하게 기대한다. feature correspondence 하나가 전혀 다른 물체를 가리키면 residual 제곱이 커져 정상점 수십 개보다 해를 더 강하게 끌 수 있다. 당시 흔한 방식처럼 전체 데이터로 fit한 뒤 가장 나쁜 점을 하나씩 제거하면, 첫 fit 자체가 오염되어 무엇이 나쁜 점인지 잘못 판단할 수 있다.

논문은 다음 상황을 목표로 한다.

- 데이터의 상당 부분이 작은 잡음이 아니라 **gross error/outlier**다.
- 어떤 관측이 inlier인지 미리 알 수 없다.
- 최소 관측 집합만으로 model hypothesis를 만들 수 있다.
- hypothesis가 맞다면 나머지 데이터 중 일관된 consensus set을 찾을 수 있다.

원 논문의 큰 응용은 알려진 3D landmark와 영상 관측으로 camera 위치를 구하는 Location Determination Problem이다. 오늘 코드는 더 작은 교육용 문제인 2D stereo point 두 쌍에서 current→previous SE(2)를 추정한다. 문제 크기는 다르지만 “최소 집합으로 가설을 세우고 전체 consensus로 검증한다”는 핵심은 같다.

## 2. 핵심 아이디어와 수학적 직관

### 데이터 전체를 먼저 믿지 않는다

RANSAC의 순서는 직관적으로 다음과 같다.

1. model을 정할 수 있는 최소 개수의 관측을 뽑는다.
2. 그 작은 집합만으로 hypothesis를 계산한다.
3. 전체 관측 중 residual이 threshold 안인 점을 consensus로 센다.
4. 충분히 큰 consensus가 생기면 그 점들로 model을 다시 fit한다.
5. 여러 hypothesis 중 가장 지지받는 model을 선택한다.

outlier가 model을 만드는 최소 집합에 포함되면 대체로 지지점이 적다. 반대로 최소 집합이 모두 inlier라면 실제 model 근처의 hypothesis가 만들어지고 많은 정상점이 한꺼번에 동의한다. 핵심은 평균을 잘 내는 것이 아니라 **누구의 동의를 받을지 먼저 찾는 것**이다.

### 필요한 반복 수

inlier 비율을 `w`, 최소 sample 수를 `s`라 하면 한 번 뽑은 최소 집합이 전부 inlier일 확률은 `w^s`다. `N`번 모두 실패할 확률은 `(1-w^s)^N`이므로 성공확률을 `p` 이상으로 만들려면

\[
N \ge \frac{\log(1-p)}{\log(1-w^s)}
\]

가 된다. `w`가 낮거나 `s`가 커지면 필요한 반복이 급격히 늘어난다. 이 식은 평균적인 성공확률을 말하지 hard deadline을 보장하지 않는다. 오늘 코드는 RT 학습 목적상 adaptive stopping 대신 **항상 최대 24개의 결정론적 hypothesis**를 평가한다. 따라서 계산량은 명확하지만 입력 ordering과 outlier 비율에 대한 성공확률은 별도로 검증해야 한다.

### threshold는 단순 상수가 아니다

RANSAC의 성능은 residual threshold에 크게 좌우된다.

- 너무 작으면 정상점도 outlier가 되어 consensus가 쪼개진다.
- 너무 크면 잘못된 model도 많은 점을 받아들인다.
- 측정마다 noise가 다르면 동일한 meter threshold도 공정하지 않다.

오늘 stereo 실습은

\[
Z=f_xB/d,\qquad \sigma_Z\approx(f_xB/d^2)\sigma_d
\]

를 이용해 먼 점에 더 큰 gate를 준다. 다만 지나치게 큰 uncertainty가 outlier 면허가 되지 않도록 상한을 둔다. 더 정교한 제품은 full 3D covariance를 residual 방향으로 투영한 Mahalanobis distance, robust likelihood, M-estimator를 함께 고려한다.

## 3. 실무 적용 가능성

RANSAC은 다음 robot perception 단계에 특히 잘 맞는다.

- stereo/RGB-D/monocular visual odometry의 feature match 검증
- PnP camera pose와 essential/fundamental matrix 추정
- LiDAR plane/line primitive 추출
- loop closure 전 geometric verification
- camera–LiDAR extrinsic calibration의 대응점 정제

실무 architecture에서는 detector/descriptor가 낸 수백·수천 match를 RT control callback에 그대로 넣으면 안 된다. front-end에서 score/grid quota로 입력 수를 제한하고, RANSAC hypothesis 수도 cap하며, 실패/퇴화 시 pose를 억지로 갱신하지 않는 health contract가 필요하다. 오늘의 `StereoRansacStats`는 input/valid/inlier/rejected 수, hypothesis 수, depth sigma, 공간 eigen ratio, queue drop, 계산시간을 한 번에 내보낸다.

독립 auditor가 truth와 estimator 내부 stats를 동일 stamp로 결합하는 것도 중요하다. pose RMSE만 좋으면 특정 seed에서 우연히 성공한 것인지, queue가 조용해서 잘 된 것인지 알 수 없다. 반대로 timing만 빠르면 틀린 model을 빨리 내는 것일 수 있다.

## 4. 한계와 제품 적용 시 주의점

### structured outlier

동일한 움직이는 차량 위 feature처럼 outlier끼리도 강한 별도 consensus를 만들 수 있다. 그 cluster가 배경보다 크면 RANSAC은 수학적으로 일관되지만 로봇 ego-motion에는 틀린 model을 선택한다. semantic mask, expected motion, IMU/wheel prior, multi-model segmentation이 필요하다.

### 퇴화 geometry

inlier 수가 많아도 점이 한 선·작은 영역·비슷한 depth에 몰리면 일부 자유도가 약하게 관찰된다. 원 논문의 consensus cardinality만 보고 health를 판단하면 이 문제를 놓친다. 오늘은 image-plane covariance eigen ratio를 추가하지만, full 6-DoF PnP에서는 Hessian/Fisher information의 singular value와 parallax도 봐야 한다.

### threshold와 calibration coupling

잘못된 focal length, baseline, rectification, 좌우 time sync는 모든 정상점에 systematic residual을 만든다. threshold를 키워 통과시키면 calibration 문제를 숨긴다. `CameraInfo` version/stamp, online residual pattern, temperature에 따른 baseline drift를 별도 감시해야 한다.

### bounded latency와 성공확률의 충돌

iteration cap을 낮추면 deadline은 좋아지지만 높은 outlier 비율에서 all-inlier sample을 놓칠 수 있다. cap을 높이면 성공확률은 오르지만 worst-case 계산량도 늘어난다. 제품에서는 다음을 함께 보고 정한다.

1. 최소 sample 크기와 실제 inlier 비율 분포
2. 허용되는 miss probability
3. frame budget과 다른 pipeline 단계의 WCET
4. 실패 시 IMU/wheel propagation 또는 safe degradation 정책

### 오늘 구현이 논문 전체를 재현하지는 않는다

오늘 코드는 48개 합성 stereo match, planar SE(2), 24개 deterministic pair, 한 번의 closed-form refinement만 쓴다. 원 논문의 camera location solver도 아니고, 일반 6-DoF stereo VO나 현대적인 LO-RANSAC/PROSAC 구현도 아니다. 목적은 RANSAC의 핵심과 bounded robotics pipeline을 작고 감사 가능한 코드로 연결하는 것이다.

## 엔지니어 관점의 결론

RANSAC의 가장 중요한 교훈은 “noise를 평균내면 된다”가 아니라 **관측 집합 자체가 오염될 수 있으므로 model과 지지집합을 함께 찾아야 한다**는 것이다. 로봇 제품에서는 여기에 세 가지를 더 붙여야 한다.

- measurement별 uncertainty와 calibration 계약
- hypothesis/input/queue의 명시적 계산 상한
- inlier 수와 별개인 observability/degeneracy 및 실패 fallback

이 세 가지가 있어야 RANSAC이 데모용 robust fit을 넘어 운영 가능한 perception component가 된다.
