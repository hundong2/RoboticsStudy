# Normal Distributions Transform (NDT)

NDT는 reference point cloud를 local Gaussian cell 집합으로 바꾸고, current cloud가 이 분포에 잘 들어맞는 pose를 찾는 registration 방법이다. Biber와 Straßer가 2D laser scan matching을 위해 제안했으며, 이후 3D voxel NDT와 LiDAR localization으로 널리 확장되었다.

## 최소 수식

cell `k`의 mean/covariance는 다음과 같다.

\[
\mu_k=\frac{1}{n_k}\sum_i p_i,\qquad
\Sigma_k=\frac{1}{n_k-1}\sum_i(p_i-\mu_k)(p_i-\mu_k)^T+\lambda I
\]

pose `ξ`로 변환한 current point `q_i(ξ)`의 cell이 `k(i)`일 때 Mahalanobis residual은 다음과 같다.

\[
d_i^2=(q_i-\mu_{k(i)})^T\Sigma_{k(i)}^{-1}(q_i-\mu_{k(i)})
\]

원 논문은 Gaussian score를 Newton 방식으로 최적화한다. 구현에 따라 negative log-likelihood/point-to-distribution cost를 Gauss–Newton 또는 다른 nonlinear solver로 최소화하기도 한다. 변형을 사용할 때는 “원 논문을 그대로 재현했다”는 표현을 피하고 목적함수와 solver를 명시한다.

## 설계 체크리스트

- map과 scan의 acquisition timestamp/frame/extrinsic/deskew를 먼저 검증한다.
- voxel/cell 크기와 최소 점 수를 센서 해상도·환경 scale에 맞춘다.
- covariance regularization, eigenvalue floor 또는 condition metric을 둔다.
- 좋은 initial guess와 coarse-to-fine/global fallback을 준비한다.
- used cell/point, score, step, Hessian condition/pivot, iteration, runtime을 노출한다.
- dynamic objects/outliers를 gating, robust loss, semantic mask로 다룬다.
- pose를 navigation에 넘기기 전에 독립 innovation/continuity supervisor를 둔다.

## 대표 실패 모드

- 복도/평면 하나뿐인 환경의 관측 불능 방향
- 너무 작은 cell의 sparse/singular covariance
- 너무 큰 cell의 서로 다른 표면 혼합
- poor initial guess에 의한 local optimum
- rolling-shutter LiDAR motion distortion, timestamp/extrinsic bias
- 반복/point/voxel 수가 입력에 따라 무제한 증가하는 latency tail

참고: [Biber & Straßer, IROS 2003](https://doi.org/10.1109/IROS.2003.1249285), [공개 원문](https://citeseerx.ist.psu.edu/document?doi=1f16244ce2e78881c7c96b33796a930ce73f7972&repid=rep1&type=pdf).
