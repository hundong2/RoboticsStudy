# 차동구동 운동학과 실차 보정

## 핵심 변수와 단위

- `v [m/s]`: `base_link` x축 선속도
- `ω [rad/s]`: `base_link` z축 yaw 각속도
- `r [m]`: 유효 wheel radius
- `L [m]`: 좌우 wheel 접촉점 사이의 유효 separation
- `ωL, ωR [rad/s]`: 좌우 wheel 각속도

## 역운동학: body twist → wheel command

```text
ωL = (v - ωL/2) / r
ωR = (v + ωL/2) / r
```

ROS의 REP-103 기준에서 `x` 전진, `z` 반시계 yaw가 양수일 때 양의 yaw 명령은 우측 wheel 속도를 더 크게 만든다. 모터 driver의 물리 배선과 joint axis가 이 부호 규약을 따르는지 낮은 속도에서 먼저 확인한다.

## 순운동학: encoder → odometry increment

```text
ΔsL = r ΔφL
ΔsR = r ΔφR
Δs  = (ΔsR + ΔsL)/2
Δθ  = (ΔsR - ΔsL)/L

x' = x + Δs cos(θ + Δθ/2)
y' = y + Δs sin(θ + Δθ/2)
θ' = wrap(θ + Δθ)
```

중간 heading을 쓰면 짧은 원호 구간에서 단순 Euler 적분보다 오차가 작다. 긴 sampling interval, 큰 slip, 불규칙 timestamp가 있으면 exact arc integration이나 측정 timestamp 기반 `dt`를 사용한다.

## 보정 순서

1. 같은 encoder count를 주고 실제 wheel 회전 방향이 ROS joint sign과 맞는지 확인한다.
2. 충분히 긴 직진 시험으로 유효 `r` 또는 좌·우 radius multiplier를 맞춘다.
3. 여러 바퀴 제자리 회전으로 유효 `L`을 맞춘다.
4. 좌/우 방향과 서로 다른 바닥 마찰에서 반복해 systematic bias와 random slip을 분리한다.
5. 평균 오차뿐 아니라 분산을 localization motion model에 반영한다.

`wheel_radius`가 틀리면 거리 scale, `wheel_separation`이 틀리면 yaw scale이 주로 틀어진다. 타이어 압력·하중·바닥 재질이 유효값을 바꿀 수 있으므로 CAD 숫자를 그대로 ground truth로 취급하지 않는다.

## ROS 2 인터페이스 연결

실제 `diff_drive_controller`는 body velocity command를 wheel velocity interface로 변환하고 hardware feedback으로 odometry를 계산할 수 있다. 명령 timeout, 속도/가속도/jerk 제한, covariance, frame id를 함께 설정해야 한다.

- [ROS 2 Control Jazzy: diff_drive_controller](https://control.ros.org/jazzy/doc/ros2_controllers/diff_drive_controller/doc/userdoc.html)
