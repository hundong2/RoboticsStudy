# 분산 역수 가중 센서 융합

같은 scalar 물리량 `x`를 두 센서가 독립적인 영평균 Gaussian noise로 측정한다고 하자.

\[
z_1=x+n_1,\quad z_2=x+n_2,\quad
n_i\sim\mathcal N(0,\sigma_i^2)
\]

최대우도이자 최소분산 선형 추정은 분산의 역수, 즉 precision을 가중치로 쓴다.

\[
\hat x=\frac{z_1/\sigma_1^2+z_2/\sigma_2^2}
{1/\sigma_1^2+1/\sigma_2^2},\qquad
\sigma_{fused}^2=\frac{1}{1/\sigma_1^2+1/\sigma_2^2}
\]

Noise가 작은 센서의 가중치가 커지는 것이 직관과 일치한다. 이는 scalar Kalman measurement update와 weighted least squares의 가장 단순한 형태다.

## 적용 전 필수 조건

- 두 값은 **같은 물리 시각과 좌표계**를 나타내야 한다.
- covariance는 단위와 축 순서가 맞고 실제 오차를 대표해야 한다.
- 두 noise가 상관되어 있으면 전체 covariance의 off-diagonal을 포함해야 한다.
- bias가 있으면 평균을 여러 번 합쳐도 bias가 사라지지 않는다.
- outlier에는 Gaussian 제곱손실 대신 gating/robust loss가 필요하다.

시간 정렬보다 먼저 이 공식을 적용하면 서로 다른 상태를 평균내는 셈이다. 고속 운동에서 timestamp 오차는 noise가 아니라 systematic residual이므로 먼저 시간 calibration을 수행한다.
