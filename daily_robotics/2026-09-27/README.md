# 2026-09-27 — PointCloud2 시간 계약과 bias-aware 3D LiDAR-IMU deskew

> **오늘의 핵심:** `PointCloud2.header.stamp + point.time`으로 각 점의 취득 시각을 복원하고, 정지 구간에서 추정한 자이로 바이어스를 뺀 뒤 `SO(3)` 자세를 적분한다. 모든 점을 scan-end 좌표계로 회전 보정하며, 6×6 오차상태 공분산과 독립 평면 감사기로 결과의 품질까지 확인한다.

## 학습 순서 (Reading Order)

1. 이 README의 **통신 구조 → 시간 계약 → 수식** 순서로 전체 흐름을 잡는다.
2. [`src/lidar_imu_simulator.cpp`](src/lidar_imu_simulator.cpp)에서 `sensor_msgs/Imu`와 `PointCloud2`의 필드/시각 계약을 읽는다.
3. [`src/imu_deskew_node.cpp`](src/imu_deskew_node.cpp)에서 고정 배열, bias 초기화, 쿼터니언 적분, 공분산 전파를 수식과 대조한다.
4. [`src/deskew_auditor.cpp`](src/deskew_auditor.cpp)에서 생산 코드와 분리된 검증 경계를 확인한다.
5. [`paper_review.md`](paper_review.md)에서 FAST-LIO2가 이 작은 실습을 어떻게 완전한 LIO/SLAM 시스템으로 확장하는지 읽는다.
6. 빌드 후 `test/smoke_test.sh`를 실행해 `AUDIT_PASS`를 직접 확인한다.

## 세 영역의 균형

| 영역 | 오늘 다루는 내용 | 실무에서 답해야 할 질문 |
|---|---|---|
| 기초 실무 | `PointCloud2`의 `fields/point_step/row_step`, `Imu` covariance, SensorDataQoS | `header.stamp`는 시작인가 끝인가? `time` 단위와 타입은 무엇인가? |
| 심화·RT | 512 IMU ring, scan당 64 IMU/128 point 상한, 단일 스레드 직렬화, callback 계측 | 메모리·반복 횟수·입력 크기의 최악 상한이 있는가? |
| 알고리즘 | gyro bias 초기화, `SO(3)` 적분, 점별 scan-end deskew, 6×6 covariance | 보정값뿐 아니라 불확실성과 독립 품질 지표도 제공하는가? |

## ROS 2 통신 구조 (`rqt_graph` 관점)

```mermaid
graph LR
    S[lidar_imu_simulator] -->|/study/imu<br/>sensor_msgs/Imu<br/>200 Hz SensorDataQoS| I((IMU))
    S -->|/study/cloud_raw<br/>PointCloud2 x y z intensity time<br/>10 Hz SensorDataQoS| R((Raw cloud))
    I --> D[imu_deskew_node<br/>bias init + SO(3) integration<br/>fixed 512/64/128]
    R --> D
    D -->|/study/cloud_deskewed<br/>scan-end frame| C((Corrected cloud))
    D -->|/study/deskew_stats<br/>bias sigma timing| T((Stats))
    R -. raw plane residual .-> A[deskew_auditor]
    C --> A
    T --> A
    A -->|5 consecutive passes| P[/study/audit_pass<br/>Transient Local Bool]
```

더 자세한 탐색형 구조도: [`architecture/architecture.html`](architecture/architecture.html). 이 HTML은 light/dark, 경로 focus, pan/zoom을 지원한다. 작성 언어는 한국어이며 고정 Viewer UI는 영어로 표시된다.

## 1. PointCloud2 시간 계약

`PointCloud2`는 점의 의미를 `fields`에 기록하고 실제 값은 `data` 바이트 배열에 담는다. 이 실습의 한 점은 정확히 20 byte다.

| offset | 타입 | 이름 | 의미 |
|---:|---|---|---|
| 0 | `float32` | `x` | 취득 순간 `lidar_link`에서의 x [m] |
| 4 | `float32` | `y` | y [m] |
| 8 | `float32` | `z` | z [m] |
| 12 | `float32` | `intensity` | 교육용 반사 강도 |
| 16 | `float32` | `time` | `header.stamp`로부터의 상대 시간 [s] |

중요한 점은 ROS 표준 메시지가 `time` 필드의 이름·단위·기준을 강제하지 않는다는 사실이다. 드라이버에 따라 `t`, `timestamp`, ns 정수, scan-end 기준일 수 있다. 따라서 실제 센서를 붙이기 전에 드라이버 문서를 확인하고 이 실습의 `findField()`/단위 검사를 어댑터로 바꿔야 한다.

## 2. 수식에서 코드까지

### 자이로 바이어스

첫 160개(200 Hz에서 0.8 s) 정지 표본에 대해 다음 평균을 구한다.

\[
\hat{b}_g = \frac{1}{N}\sum_{k=1}^{N}\omega_{m,k}
\]

운동 중 각속도는 `omega_corrected = measured_omega - gyro_bias_`다. 실제 제품에서는 가속도 norm만으로 정지를 선언하지 말고 wheel speed, 접촉, 분산 등 다중 조건과 캘리브레이션 실패 상태를 둬야 한다.

### 쿼터니언 적분과 점 보정

두 IMU 표본의 평균 각속도로 자세 증가량을 만든다.

\[
q_{k+1}=q_k\otimes \operatorname{Exp}((\omega_k-\hat b_g)\Delta t)
\]

점 `i`가 취득된 순간의 자세를 `R_i`, 스캔 끝 자세를 `R_e`라 하면 정지한 환경의 같은 점을 끝 좌표계로 옮기는 식은 다음과 같다.

\[
{}^{e}p_i = R_e^\top R_i\,{}^{i}p_i
\]

코드의 `(orientation_end.conjugate() * orientation_point).rotate(point)`가 바로 이 식이다. 오늘 실습은 **회전 deskew만** 구현한다. 이동하는 플랫폼에서는 위치/속도와 LiDAR-IMU 외부 파라미터까지 포함해 `R_e^T(R_i p_i + t_i - t_e)`를 써야 한다.

### 6×6 오차상태 공분산

오차상태를 `delta_x=[delta_theta, delta_b_g]`로 두고 작은 각도에서 다음 모델을 쓴다.

\[
F=\begin{bmatrix}I&-I\Delta t\\0&I\end{bmatrix},\qquad
P_{k+1}=FP_kF^\top+Q
\]

`rotation_sigma_rad = sqrt(trace(P_theta_theta)/3)`는 현재 스캔 회전 보정의 대표 1σ다. 이는 완전한 일관성 검증(NEES/NIS)이나 LiDAR measurement update를 대체하지 않는다.

## 3. 실시간 설계 경계

- IMU 저장은 `std::array<ImuSample, 512>` ring이며 오래된 표본을 덮어쓴다.
- 한 스캔은 최대 64개 IMU와 128개 점만 처리한다. `orientationAt()`의 연산 상한은 `128 × 63` 구간이다.
- `SingleThreadedExecutor`를 전제로 IMU/cloud callback이 같은 배열을 직렬 접근한다. 멀티스레드로 바꿀 때는 SPSC queue나 명시적 동기화가 필요하다.
- 수치 경로는 고정 배열이지만 `PointCloud2` 복사와 DDS publish는 직렬화/할당을 포함할 수 있다. 따라서 이 예제는 hard RT를 주장하지 않는다.
- `callback_us`는 Docker에서 관측한 샘플일 뿐 WCET 증명이 아니다. 실제 배포는 `ros2_tracing`, CPU affinity, page fault, RMW/loaned-message 지원까지 측정해야 한다.

## 4. 독립 감사 계약

시뮬레이터는 세계 좌표 `x=6 m` 평면을 16×8 점으로 만든다. 움직이는 센서에서 받은 raw cloud는 휘어 보이고, 정확히 보정된 점은 다시 한 평면에 놓인다. 감사 노드는 다음을 5회 연속 확인한다.

- 보정 평면 평균 절대 잔차 `< 5 mm`
- 보정 잔차 `< raw 잔차의 20%`
- 세 축 gyro bias 최대 오차 `< 0.001 rad/s`
- `0 < rotation_sigma < 0.01 rad`
- 128 points, 18–32 IMU samples, truncation 0
- 관측 callback `< 5 ms`

임계값은 이 결정론적 교육 시뮬레이터용이다. 실제 센서 임계값은 range noise, target geometry, 회전율, 온도별 bias 통계로 정해야 한다.

## 빌드와 실행

ROS 2 Jazzy 작업공간의 `src/` 아래에 이 폴더를 두었다고 가정한다.

```bash
colcon build --packages-select daily_robotics_2026_09_27 --event-handlers console_direct+
source install/setup.bash
ros2 launch daily_robotics_2026_09_27 study.launch.py
```

별도 터미널에서:

```bash
source install/setup.bash
ros2 topic echo --once /study/deskew_stats
ros2 topic echo --once /study/audit_pass
```

자동 검증:

```bash
source install/setup.bash
ROS_DOMAIN_ID=127 ros2 run daily_robotics_2026_09_27 smoke_test.sh
```

## 의도적으로 남긴 한계

- 병진 운동, 가속도/중력 정렬, IMU-LiDAR extrinsic, clock offset을 생략했다.
- 정지 초기화 실패/재초기화 상태 머신과 온도 의존 bias 모델이 없다.
- raw point-to-map update, nearest-neighbor search, loop closure가 없으므로 odometry/SLAM이 아니다.
- `float32 time`은 100 ms 범위에는 충분하지만 장시간 절대 timestamp에는 부적절하다.
- 한 평면은 특정 축 회전/시간 오류에 둔감할 수 있다. 실제 검증은 여러 방향의 plane/edge와 ground truth trajectory를 사용한다.

## 참고 자료

- [ROS 2 Jazzy `sensor_msgs/PointCloud2`](https://docs.ros.org/en/jazzy/p/sensor_msgs/msg/PointCloud2.html)
- [ROS 2 Jazzy `sensor_msgs/PointField`](https://docs.ros.org/en/jazzy/p/sensor_msgs/msg/PointField.html)
- [ROS 2 Jazzy `sensor_msgs/Imu`](https://docs.ros.org/en/jazzy/p/sensor_msgs/msg/Imu.html)
- [ROS 2 QoS concepts](https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html)
- [FAST-LIO2 arXiv](https://arxiv.org/abs/2107.06829)
- [FAST-LIO2 IEEE T-RO DOI](https://doi.org/10.1109/TRO.2022.3141876)

