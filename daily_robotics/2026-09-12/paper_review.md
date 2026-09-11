# 논문 리뷰 — ORB-SLAM: A Versatile and Accurate Monocular SLAM System

## 서지 정보

- **저자:** Raúl Mur-Artal, J. M. M. Montiel, Juan D. Tardós
- **학술지:** IEEE Transactions on Robotics, Vol. 31, No. 5, pp. 1147–1163, 2015
- **DOI:** [10.1109/TRO.2015.2463671](https://doi.org/10.1109/TRO.2015.2463671)
- **공개 원고:** [arXiv:1502.00956](https://arxiv.org/abs/1502.00956)
- **저자 구현:** [raulmur/ORB_SLAM](https://github.com/raulmur/ORB_SLAM)

오늘은 최신 논문 대신 Visual SLAM 제품 아키텍처의 기준점을 만든 근간 논문을 골랐다. 2015년 논문이지만 “tracking만 빠르게 만들면 SLAM인가?”라는 질문에 front-end, local mapping, loop closing, relocalization, map 관리 전체로 답하기 때문에 오늘의 작은 VO 실습이 어디까지이고 무엇이 빠졌는지 판단하기 좋다.

## 1. 해결하려는 문제

단안 camera 한 대로 매 frame의 camera pose와 sparse 3D map을 실시간 추정하려면 서로 충돌하는 요구를 동시에 만족해야 한다.

- Tracking은 frame rate를 놓치지 않아야 한다.
- 새 keyframe/map point를 계속 추가하면 map과 최적화 비용이 끝없이 커진다.
- 같은 장소로 돌아왔을 때 누적 drift를 닫아야 한다.
- Tracking을 잃어도 기존 map에서 다시 위치를 찾아야 한다.
- 단안 camera는 depth와 절대 scale을 직접 보지 못하고, 초기화 조건도 까다롭다.

이전 연구에도 bundle adjustment, keyframe, place recognition 같은 좋은 부품은 있었다. ORB-SLAM의 중요한 공헌은 이 부품들을 한 시스템 안에서 같은 ORB feature와 graph 표현으로 연결하고, map이 장기 실행 중 통제 불가능하게 자라지 않도록 생성과 제거 정책까지 설계한 데 있다.

## 2. 핵심 아이디어와 수학적 직관

### 같은 ORB feature를 모든 단계에서 재사용

Tracking, map point 생성, relocalization, loop detection이 서로 다른 feature를 쓰면 descriptor 변환과 일관성 관리가 복잡해진다. ORB-SLAM은 rotation/scale에 비교적 강하고 계산이 빠른 ORB feature를 공통 화폐처럼 사용한다. 그 결과 현재 frame의 local match와 과거 장소를 찾는 bag-of-words 검색을 같은 descriptor로 연결한다.

### 세 역할을 병렬화

- **Tracking:** 현재 frame pose를 예측하고 map point reprojection match로 pose를 다듬는다.
- **Local Mapping:** 새 keyframe과 map point를 만들고 local bundle adjustment를 수행한다.
- **Loop Closing:** bag-of-words로 과거 장소 후보를 찾고 geometric consistency를 검증한 뒤 loop를 닫는다.

핵심은 모든 일을 frame callback 하나에 넣지 않는 것이다. Tracking의 deadline과 더 무거운 map/loop 최적화의 처리량을 분리한다. 다만 thread를 나눴다는 사실만으로 hard RT가 되는 것은 아니며 shared map lock, 최적화 시간 tail, memory allocation은 별도 분석 대상이다.

### Bundle Adjustment는 reprojection error를 줄인다

3D map point `X_j`를 camera pose `T_i`와 camera projection `π`로 image에 투영한 예측은 `π(T_i X_j)`다. 실제 관측 pixel `u_ij`와의 차이를 robust cost로 줄인다.

\[
\min_{\{T_i\},\{X_j\}}
\sum_{(i,j)\in\mathcal O}
\rho\left(\left\|u_{ij}-\pi(T_iX_j)\right\|_{\Sigma_{ij}}^2\right)
\]

Pose와 point를 함께 최적화하면 정확하지만 변수와 관측이 많다. ORB-SLAM은 현재와 강하게 연결된 **local covisibility graph**만 자주 최적화하고, loop closure에는 spanning tree·loop edge·강한 covisibility edge로 구성한 더 성긴 Essential Graph를 사용해 계산량을 통제한다.

### “많이 만들고 엄격히 버린다”

초기에 keyframe/map point 후보를 넉넉히 만들되, 다른 frame에서도 안정적으로 관측되지 않는 point와 중복 keyframe은 제거한다. 이 culling은 단순 메모리 절약이 아니다. 나쁜 landmark가 pose 최적화를 오염시키는 것을 막고, 장소의 실제 시각 내용이 변하지 않으면 map 크기가 무작정 늘지 않게 한다.

### 단안의 scale과 loop correction

단안 영상만으로는 world를 `s`배 키우고 camera translation도 `s`배 키운 해를 구분할 수 없다. 그래서 절대 metric scale은 관측 불가능하고, loop correction에는 scale을 포함한 similarity transform `Sim(3)`가 필요하다. 오늘 실습은 LiDAR metric depth를 feature에 결합했기 때문에 이 scale 모호성을 의도적으로 제거하고 평면 `SE(2)` 정합에 집중한다.

## 3. 실무 적용 가능성과 한계

### 적용할 설계 원칙

1. **Deadline이 다른 일을 분리한다.** Tracking, local optimization, global correction을 한 callback에 직렬로 넣지 않는다.
2. **하나의 관측 표현을 여러 단계에서 재사용한다.** Feature ID와 frame/stamp 계약을 front-end부터 back-end까지 유지한다.
3. **Map 생성만큼 제거 정책을 먼저 설계한다.** CPU/메모리 상한과 품질은 culling 정책에 달려 있다.
4. **Place recognition 뒤에 geometric verification를 둔다.** 비슷해 보인다는 이유만으로 loop edge를 넣으면 map 전체가 접힐 수 있다.
5. **Tracking loss를 정상 상태로 취급한다.** Relocalization, reset, 안전 정지 경로를 제품 상태 머신에 넣는다.

### 논문/구현의 한계와 제품화 위험

- **Texture·motion 의존:** 저자 구현도 낮은 texture, 큰 동적 물체, 초기화 시 translation 부족이나 pure rotation을 대표 실패 조건으로 든다.
- **단안 metric scale 부재:** wheel/IMU/LiDAR/GNSS 같은 추가 센서나 알려진 크기 prior가 없으면 m 단위 제어에 바로 쓸 수 없다.
- **동적 장면:** 움직이는 사람/차량 feature가 다수면 정적 world 가정이 깨진다. Semantic/dynamic masking과 motion consistency가 필요하다.
- **Calibration/time sync:** Camera intrinsic, distortion, rolling shutter, camera–IMU/LiDAR extrinsic과 timestamp offset이 틀리면 알고리즘 튜닝으로 해결되지 않는다.
- **평균 실시간 ≠ deadline 보장:** Dataset 평균 FPS는 worst-case callback/optimization latency나 actuator deadline의 증명이 아니다.
- **라이선스:** 공개 ORB-SLAM 구현은 GPLv3이며 closed-source 제품에는 별도 검토/라이선스 전략이 필요하다.
- **Fail-safe 부재:** Pose quality 저하를 감지해 감속/정지시키는 safety monitor는 SLAM 정확도와 별도의 시스템 기능이다.

## 오늘 실습과의 연결

| ORB-SLAM 전체 시스템 | 오늘 구현 | 의도적으로 생략한 것 |
|---|---|---|
| ORB detection/description과 matching | 이미 ID가 붙은 camera bearing | Image/OpenCV, descriptor, optical flow |
| Monocular geometry와 map depth | LiDAR metric XY feature | Triangulation, monocular initialization |
| Tracking pose optimization | 두 frame의 bounded SE(2) closed form | SE(3), reprojection BA, covariance |
| Robust match 관리 | bearing dot gate + residual 2-pass | RANSAC, robust kernel, uncertainty weighting |
| Local mapping | 없음 | Keyframe/map point 생성과 culling |
| Loop closing/relocalization | 없음 | BoW, covisibility/Essential Graph, Sim(3) |

따라서 오늘 노드를 “ORB-SLAM 구현”이라고 부르면 안 된다. 정확한 이름은 **동기화된 metric feature를 사용하는 bounded frame-to-frame Visual Odometry skeleton**이다. 다음 단계는 bounded RANSAC과 SE(3) PnP를 추가하고, 그 다음에 keyframe/local map/loop closure를 별도 실행 경계로 확장하는 것이다.

## 엔지니어가 남겨야 할 검증 질문

- Camera와 LiDAR stamp가 정말 같은 clock에서 나왔는가?
- Extrinsic calibration 오차가 residual 분포에 어떻게 나타나는가?
- Feature 수, outlier 비율, motion blur별 tracking failure 경계는 어디인가?
- Tracing을 켰을 때 p99/p99.9 callback latency가 얼마나 바뀌는가?
- Tracking이 끊긴 순간 motion controller는 마지막 pose를 얼마나 오래 신뢰하는가?
- Loop edge 한 개가 틀렸을 때 global map과 안전 영역이 어떻게 망가지는가?
