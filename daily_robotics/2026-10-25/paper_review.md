# 논문 리뷰 — Good Features to Track

## 서지 정보

- Jianbo Shi, Carlo Tomasi, **“Good Features to Track,”** IEEE Conference on Computer Vision and Pattern Recognition (CVPR), pp. 593–600, 1994.
- DOI: [10.1109/CVPR.1994.323794](https://doi.org/10.1109/CVPR.1994.323794)
- 공개 원고: [Cornell Technical Report TR 93-1399](https://users.cs.duke.edu/~tomasi/papers/shi/TR_93-1399_Cornell.pdf)
- 계보: Lucas–Kanade의 gradient 기반 registration을 바탕으로 “어떤 patch를 추적해야 하는가”와 “언제 그 track을 버려야 하는가”를 연결한 근간 연구다.

## 1. 해결하려는 문제

영상에서 점을 많이 검출하는 것과 **오랫동안 정확히 추적할 수 있는 점을 고르는 것**은 다르다. 평탄한 patch는 어느 방향으로 움직여도 밝기 변화가 거의 없어 motion을 정할 수 없다. 긴 edge 위 점은 edge 수직 방향 motion만 알 수 있고 edge 방향은 모호하다(aperture problem). 강한 corner도 시간이 지나 occlusion, depth boundary, reflection, 큰 viewpoint 변화가 생기면 더는 같은 3D 점을 나타내지 않을 수 있다.

당시의 단순 cornerness나 local contrast 기준은 tracker가 실제로 풀어야 하는 최적화 문제와 직접 연결되지 않는 경우가 있었다. 이 논문은 두 질문을 하나의 관점으로 다룬다.

1. 현재 translation/affine tracker가 수치적으로 잘 풀 수 있는 patch는 무엇인가?
2. 처음 보았던 feature가 지금도 같은 물리적 표면을 나타내는지 어떻게 감시할 것인가?

로봇 엔지니어 관점에서 이는 feature detector만의 문제가 아니다. visual odometry/SLAM의 association 품질, pose conditioning, failure detection을 결정하는 frontend 계약이다.

## 2. 핵심 아이디어와 수학적 직관

### Tracker의 normal matrix로 feature 품질을 정의한다

작은 motion과 brightness constancy를 가정하면 patch registration residual을 1차 선형화할 수 있다. Translation `d=[dx,dy]^T`를 풀 때 나타나는 2×2 normal matrix는

\[
G=\sum_{p\in W}\nabla I(p)\nabla I(p)^T
=\sum_{p\in W}
\begin{bmatrix}I_x^2&I_xI_y\\I_xI_y&I_y^2\end{bmatrix}
\]

이다. `G`의 고윳값은 patch가 두 image 방향 motion에 얼마나 정보를 주는지 나타낸다.

- 두 고윳값이 모두 작다 → 평탄 영역, motion을 거의 관찰할 수 없다.
- 하나만 크다 → edge, edge를 따라가는 motion이 모호하다.
- 둘 다 크다 → 두 방향 모두 구별되는 corner/texture다.

따라서 작은 고윳값 `min(λ1,λ2)`이 threshold보다 큰 patch를 고르면 tracker의 최악 방향 uncertainty를 직접 제한할 수 있다. “사람 눈에 corner처럼 보인다”가 아니라 **tracker가 푸는 선형 시스템이 잘 conditioned되는가**가 선택 기준이라는 점이 핵심이다.

### 짧은 추적과 긴 시간 감시에 다른 motion model을 쓴다

연속 frame 사이 작은 motion에는 translation model이 단순하고 안정적이다. 하지만 첫 frame과 멀어진 현재 frame을 비교하면 viewpoint 변화가 누적되어 translation만으로 appearance 차이를 설명하기 어렵다. 논문은 affine deformation을 추정해 feature의 장기 appearance dissimilarity를 감시한다. 잔차가 커지면 occlusion/disocclusion 또는 물리적 점의 불일치를 의심하고 track을 버린다.

이 구분은 실무에서 중요하다. 더 복잡한 model을 항상 쓰는 것이 좋은 것이 아니라, 작은 baseline tracking에는 낮은 자유도 model을 쓰고 장기 생존성 판단에는 더 표현력 있는 model을 쓴다.

### 오늘 코드와 연결

오늘의 `shi_tomasi_score()`는 3×3 window의 `G`에서 작은 고윳값을 closed form으로 구한다. 그러나 correspondence 단계는 논문의 iterative affine KLT를 그대로 구현하지 않는다.

- 오늘 구현: 5×5 SAD, 정수 ±4 px exhaustive search, forward-backward 1 px gate
- 원 논문: gradient/Newton-Raphson 계열 registration, translation과 affine model, 장기 dissimilarity monitoring

즉 detector의 수학은 직접 가져오되 tracker는 계산 상한을 눈으로 셀 수 있는 작은 교육용 block matcher로 바꿨다. 이 차이를 숨기지 않아야 성능 주장도 정확해진다.

## 3. 실무 적용 가능성과 한계

### 적용 가치

- **VO/SLAM frontend:** 잘 conditioned된 feature만 geometry solver에 보내 pose normal equation의 불안정을 줄인다.
- **Track lifecycle:** 단순 age보다 appearance/forward-backward/reprojection 품질로 feature를 폐기한다.
- **계산 예산:** score 상위 N개, image grid별 quota를 사용하면 downstream RANSAC/BA workload를 제한할 수 있다.
- **진단 가능성:** feature count 하나가 아니라 minimum eigenvalue, spatial spread, residual, FB error, inlier ratio를 telemetry로 내보낼 수 있다.
- **센서 융합:** visual 품질이 나쁠 때 covariance를 키우거나 estimator의 visual update를 중지하는 입력으로 쓸 수 있다.

### 한계와 실패 조건

- brightness constancy는 auto exposure, flicker, blur, specular surface에서 깨진다.
- 작은 motion linearization은 빠른 회전/큰 inter-frame displacement에서 실패한다. pyramid나 motion prediction이 필요하다.
- affine patch model도 큰 3D parallax, nonrigid object, occlusion을 완전히 설명하지 못한다.
- 좋은 local texture가 올바른 3D correspondence를 보장하지 않는다. epipolar/PnP/reprojection geometry gate가 뒤따라야 한다.
- feature가 많아도 image 한쪽 또는 한 plane에 몰리면 pose가 퇴화한다. 공간 분포와 geometry conditioning을 별도로 감시해야 한다.
- 원 논문은 bounded latency나 hard-RT를 증명하지 않는다. 제품 구현은 image size, pyramid level, iteration, feature 수, RANSAC hypothesis 수를 명시적으로 제한해야 한다.

### 제품화 판단

Shi–Tomasi score는 지금도 이해하기 쉽고 강력한 baseline이지만, “좋은 feature”의 정의는 최종 task까지 포함해야 한다. tracker에 좋은 점이 VIO scale/rotation 관측에 충분하지 않을 수 있고, 정적 배경보다 동적 물체에 더 많은 corner가 생길 수도 있다. 제품 pipeline은 다음을 함께 묶어야 한다.

1. calibration/timestamp/exposure/rolling-shutter 계약
2. spatially balanced feature selection
3. bounded tracking과 track lifecycle
4. geometric outlier rejection
5. pose uncertainty와 tracking state
6. relocalization/loop closure와 독립 failure monitor

논문의 가장 오래가는 교훈은 특정 corner 식보다 **선택 기준을 downstream estimator가 실제로 푸는 수학과 연결하라**는 설계 원칙이다.
