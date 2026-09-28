# Fixed-Lag Factor Graph와 강건 추정

## Factor graph 관점

상태 `X`와 측정 `z_k`의 negative log likelihood를 더하면 nonlinear least squares가 된다.

\[
X^*=\arg\min_X\sum_k \rho_k\left(\|h_k(X_k)-z_k\|^2_{\Sigma_k^{-1}}\right)
\]

각 factor가 소수 변수만 참조하므로 Jacobian/Hessian은 sparse하다. Prior는 gauge freedom을 없애고, odometry/IMU는 연속 상태를 연결하며, GNSS/landmark는 global drift를 제한한다.

## Huber IRLS

Normalized residual `s`와 threshold `δ`에 대해 Huber influence를 IRLS weight로 쓰면

\[
w(s)=\begin{cases}1,&s\le\delta\\ \delta/s,&s>\delta\end{cases}
\]

이고 각 반복에서 information을 `wΣ⁻¹`로 조정한다. Sensor covariance가 잘못되면 normalized residual도 잘못되므로 robust loss가 calibration을 대신할 수 없다.

## Fixed lag와 marginalization

오래된 변수 `o`와 남길 변수 `r`의 normal equation을

\[
\begin{bmatrix}H_{oo}&H_{or}\\H_{ro}&H_{rr}\end{bmatrix}
\begin{bmatrix}\Delta_o\\\Delta_r\end{bmatrix}=
\begin{bmatrix}g_o\\g_r\end{bmatrix}
\]

로 나누면 오래된 변수를 제거한 prior는

\[
H_m=H_{rr}-H_{ro}H_{oo}^{-1}H_{or},\qquad
g_m=g_r-H_{ro}H_{oo}^{-1}g_o
\]

이다. 이것이 Schur complement다. 메모리와 계산량은 lag에 의해 제한되지만, marginal prior는 제거 당시 선형화점의 정보이므로 이후 큰 상태 이동에서 inconsistency가 생길 수 있다.

## 실무 선택

- 전체 history와 loop closure 수정이 중요하면 iSAM2 같은 incremental smoother
- 최근 수 초의 상태/바이어스만 필요하면 fixed-lag smoother
- 아주 낮은 지연과 작은 상태가 핵심이면 EKF/ESKF

선택은 “어느 알고리즘이 더 최신인가”보다 상태 규모, nonlinearity, 과거 수정 필요성, 최악 update 비용, 구현 검증 능력으로 결정한다.

## 실패 모드

- 잘못된 data association은 robust loss로도 구조적 오류를 남길 수 있다.
- 연속 outlier는 서로를 지지해 정상 factor처럼 보일 수 있다.
- Marginalization이 dense prior를 만들어 sparsity를 해칠 수 있다.
- Gauge/observability 문제가 있으면 damping으로 숫자만 풀려도 물리적으로 유일하지 않다.
- 평균 update 시간은 hard real-time deadline 증거가 아니다.

## 참고

- [Kaess et al., iSAM2, IJRR 2012](https://doi.org/10.1177/0278364911430419)
- [GTSAM IncrementalFixedLagSmoother](https://gtsam.org/doxygen/a05927.html)
