# Pose-graph SLAM 최적화

## 모델

Vertex `x_i`는 keyframe pose, edge `z_ij`는 센서가 측정한 `i→j` 상대변환이다. 예측 `h(x_i,x_j)`와 측정의 차이를 Lie-group 의미의 residual `e_ij`로 만들고 정보행렬 `Ω_ij`로 가중한다.

\[
F(x)=\sum e_{ij}^{T}\Omega_{ij}e_{ij}
\]

Gauss–Newton은 매 iteration에서 residual을 선형화하여 `H Δx=-b`를 풀고 pose를 갱신한다. 첫 pose 고정 또는 prior factor로 global translation/rotation gauge freedom을 제거해야 한다.

## Front-end와 back-end 계약

- Front-end: keyframe 선택, odometry/scan matching, loop 후보 탐색, geometric verification, 상대변환과 covariance 산출
- Back-end: graph 저장, robust cost, sparse nonlinear optimization, marginal/covariance와 상태 진단

Back-end가 아무리 좋아도 잘못된 loop closure edge를 자동으로 진실과 구분하지는 못한다. Robust kernel, switchable constraint, consistency check와 운영상 rollback이 중요하다.

## 구현 단계

1. SE(2)에서 3-DoF pose와 상대변환부터 구현한다.
2. Analytic Jacobian을 finite difference로 단위 시험한다.
3. Dense solver로 작은 graph를 검증한 뒤 sparse block solver로 옮긴다.
4. Synthetic loop에서 cost와 closure gap이 감소하는지 확인한다.
5. Outlier loop, 큰 yaw error, disconnected graph, singular Hessian을 주입한다.
6. Production에서는 incremental update와 graph 크기/latency budget을 설계한다.

## 참고

- [Grisetti et al., “A Tutorial on Graph-Based SLAM,” 2010](https://doi.org/10.1109/MITS.2010.939925)
