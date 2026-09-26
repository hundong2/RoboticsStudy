# Bias-aware IMU 회전 deskew와 공분산

## 최소 상태

회전 deskew의 최소 오차상태는 `delta_x=[delta_theta, delta_b_g]`다. 측정 자이로는 `omega_m = omega + b_g + n_g`로 모델링한다. 정지 초기화에서 bias 평균을 구할 수 있지만, 진동·온도·미세 운동이 있으면 이 가정이 깨진다.

## 자세 적분

\[
q_{k+1}=q_k\otimes\operatorname{Exp}((\omega_m-\hat b_g)\Delta t)
\]

작은 각도만 Euler로 더하지 말고 단위 쿼터니언 또는 회전행렬 지수지도를 사용한다. 여러 번 곱한 뒤 정규화해 수치 drift를 억제한다. 점별 보정은 `p_e=R_e^T R_i p_i`이며 병진까지 있으면 `p_e=R_e^T(R_i p_i+t_i-t_e)`다.

## 공분산

작은 각도 오차 모델에서 `F(theta,bias)=-I dt`이고 `P_next=F P F^T+Q`로 전파한다. 회전 sigma가 작아도 clock offset, extrinsic, vibration rectification 같은 모델 밖 오차는 포함되지 않는다. 제품 진단은 covariance와 geometric residual, 시간 동기 상태를 함께 봐야 한다.

## 확장 순서

1. accelerometer bias와 gravity를 포함한 15-state ESKF
2. 위치·속도 적분과 LiDAR-IMU extrinsic
3. point-to-plane measurement update
4. online time offset/extrinsic calibration
5. NEES/NIS와 ground-truth 기반 일관성 검증

