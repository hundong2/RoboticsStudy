# On-Manifold IMU Preintegration

## 왜 필요한가

카메라 keyframe 사이에는 수십~수백 IMU 샘플이 존재한다. 각 샘플을 graph state/factor로 만들지 않고 `ΔR, Δv, Δp` 한 묶음으로 압축하면 graph 크기와 반복 계산을 줄일 수 있다.

## 핵심 불변조건

1. 적분 결과는 world가 아니라 시작 keyframe local frame에 표현한다.
2. 회전은 `SO(3)`의 `Exp/Log`로 다룬다.
3. 측정 timestamp 차이로 `dt`를 계산하고 arrival time을 섞지 않는다.
4. bias 선형화점과 `J_bias`를 함께 저장한다.
5. preintegrated covariance와 raw IMU noise density/random walk 단위를 구분한다.

## 기본식

\[
R_j=R_i\Delta R_{ij},\quad
v_j=v_i+g\Delta t+R_i\Delta v_{ij}
\]

\[
p_j=p_i+v_i\Delta t+\frac12g\Delta t^2+R_i\Delta p_{ij}
\]

Bias 1차 보정은 `Δ(b)≈Δ(b̄)+J_b(b-b̄)`다. 변화량이 선형 근사 범위를 벗어나면 raw IMU를 다시 preintegrate한다.

## Marginalization과 함께 쓸 때

- 오래된 state를 제거하기 전 factor graph의 정보를 Schur complement prior로 남긴다.
- prior는 제거 당시 선형화점에 고정되므로 gauge/관측불가능 방향을 잘못 잠그지 않아야 한다.
- dense prior는 sparsity fill-in을 만든다. window 크기와 variable ordering을 함께 설계한다.

## 제품 체크리스트

- hardware time sync와 camera–IMU time offset
- `T_camera_imu` extrinsic과 축 방향
- gravity 포함/제거 계약
- gyro/accel saturation과 온도 bias
- covariance propagation 단위와 Allan variance 파라미터
- delayed/out-of-order/drop 정책
- NEES/NIS 및 ground-truth trajectory 검증

## 관련 실습과 논문

- `daily_robotics/2026-10-24`: planar `SO(2)` preintegration, gyro-bias Jacobian, fixed-lag Schur marginalization
- Forster et al., [RSS 2015 proceedings](https://www.roboticsproceedings.org/rss11/p06.html), DOI 10.15607/RSS.2015.XI.006
