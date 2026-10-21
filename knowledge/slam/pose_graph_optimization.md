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

## Switchable constraint로 false loop 격리

일반 pose graph는 edge topology를 정답으로 가정한다. 하지만 place recognition이 만든 false positive loop 하나가 모든 pose를 잘못된 절충점으로 끌 수 있다. 의심 가능한 loop마다 `s_ij`를 추가하면 topology 자체를 연속 최적화할 수 있다.

\[
\min_{X,S}\sum\|r^{odom}\|^2_{\Omega_o}
+\sum\|s_{ij}r^{loop}_{ij}\|^2_{\Omega_l}
+\sum\|1-s_{ij}\|^2_{\Xi},\quad 0\le s_{ij}\le1
\]

- 정상 loop: pose와 잘 맞으므로 prior가 `s≈1`을 유지한다.
- false loop: residual 비용이 prior보다 커져 `s≈0`으로 내려간다.
- `s`를 끄는 것은 그 edge의 information을 0에 가깝게 낮추는 해석과 같다.

실무에서는 switch만 믿지 말고 front-end geometric verification, switch/residual diagnostics, graph rollback, 독립 안전 계층을 함께 둔다. prior information이 지나치게 강하면 false loop를 끄지 못하고, 약하면 정상 loop도 제거한다. 대규모 graph에는 dense solver 대신 sparse incremental solver와 graph-size/latency budget이 필요하다.

- [Sünderhauf and Protzel, “Switchable Constraints for Robust Pose Graph SLAM,” IROS 2012](https://doi.org/10.1109/IROS.2012.6385590)
