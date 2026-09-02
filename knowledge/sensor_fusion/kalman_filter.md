# Kalman Filter — 로보틱스 엔지니어를 위한 누적 노트

## 언제 쓰는가

칼만 필터는 시간에 따라 변하는 숨은 상태(위치, 속도, 자세 오차 등)를 **운동 모델의 예측**과 **잡음이 있는 센서 측정**으로 추정한다. 선형 모델과 가우시안 잡음 가정에서 평균제곱오차를 최소화하는 재귀 추정기다.

## 변수의 뜻

```text
x : 추정하려는 상태 벡터
P : 상태 추정 오차의 공분산(불확실성)
F : 이전 상태를 다음 상태로 옮기는 전이 모델
Q : 모델이 설명하지 못하는 과정 잡음 공분산
z : 센서 측정
H : 상태를 센서가 보는 공간으로 투영하는 행렬
R : 센서 측정 잡음 공분산
K : 예측과 측정을 얼마나 섞을지 정하는 Kalman gain
```

## 두 단계

### 1. Predict

```text
x_k^- = F x_(k-1) + B u_k
P_k^- = F P_(k-1) F^T + Q
```

시간이 흐르면 상태는 운동 모델대로 이동하고, 모델이 완벽하지 않으므로 불확실성 `P`는 보통 커진다.

### 2. Correct

```text
y_k = z_k - H x_k^-                    # innovation/residual
S_k = H P_k^- H^T + R                  # innovation covariance
K_k = P_k^- H^T S_k^-1                 # Kalman gain
x_k = x_k^- + K_k y_k
P_k = (I - K_k H) P_k^-                # 기본형
```

수치 안정성이 중요하면 covariance 보정에 Joseph form을 쓴다.

```text
P_k = (I-KH)P_k^-(I-KH)^T + K R K^T
```

## 1D 등속도 예제

상태와 위치 센서는 다음과 같다.

```text
x = [position, velocity]^T
F = [[1, dt],
     [0,  1]]
H = [1, 0]
```

즉, 측정은 위치만 보지만 연속된 위치 변화와 모델을 통해 속도도 추정한다. 백색 가속도 잡음 분산을 `sigma_a^2`라 하면 흔히 다음 Q를 쓴다.

```text
Q = sigma_a^2 * [[dt^4/4, dt^3/2],
                 [dt^3/2, dt^2  ]]
```

## 튜닝 감각

- `Q ↑`: 모델 불신. 급격한 운동을 빨리 따라가지만 추정이 거칠어질 수 있다.
- `R ↑`: 센서 불신. 결과가 부드럽지만 실제 변화에 늦게 반응한다.
- `P0`: 초기 상태를 얼마나 모르는지 표현한다. 지나치게 작으면 초기 오차를 오래 고집한다.
- 단위 확인: 위치가 m이면 위치 분산은 m², 가속도 잡음 분산은 (m/s²)²다.

튜닝은 eye-balling만 하지 말고 ground truth가 있는 rosbag에서 RMSE, phase lag, innovation 분포를 함께 본다. innovation이 장기간 0 중심이 아니면 센서 bias 또는 모델 누락을 의심한다.

## 자주 하는 실수

1. 센서가 보고한 표준편차 `sigma`를 `R`에 그대로 넣는다. `R`에는 분산 `sigma²`가 들어간다.
2. 메시지 도착 시각을 센서 측정 시각으로 착각한다. 가능하면 `header.stamp`를 사용한다.
3. 서로 강하게 상관된 측정을 독립 센서처럼 연속 보정한다. 정보가 중복되어 P가 과도하게 작아진다.
4. 쿼터니언을 선형 상태처럼 바로 평균낸다. 자세는 manifold를 고려한 error-state EKF 등이 필요하다.
5. covariance 대칭성과 양의 준정부호를 확인하지 않는다. Joseph form, 대칭화, 고유값 점검을 사용한다.

## 확장 경로

- 비선형 모델: Extended Kalman Filter(EKF), Unscented Kalman Filter(UKF)
- IMU+GNSS: bias를 상태에 포함한 error-state EKF
- SLAM: 상태/공분산 크기 증가에 대비한 sparse factor graph
- 비가우시안/다봉 분포: particle filter 또는 mixture model

## 연결 실습

- [`../../daily_robotics/2026-09-02/src/rt_kalman_fusion.cpp`](../../daily_robotics/2026-09-02/src/rt_kalman_fusion.cpp)
