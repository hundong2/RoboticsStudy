# 2026-09-21 — TF 시간 계약, LiDAR Deskew, 불확실도 지도화

## 오늘의 핵심 요약

- **기초 실무:** `LaserScan.header.stamp`, `time_increment`, `frame_id`와 TF2의 exact-time lookup을 하나의 시간 계약으로 연결한다.
- **심화·RT:** 스캔당 TF 조회를 3회로 제한하고, 최대 181개 광선·광선당 100개 셀만 처리하는 bounded hot path를 설계한다.
- **알고리즘:** 스캔 시작/끝 `SE(2)` 포즈를 최단 각도로 보간해 rolling scan을 deskew하고, 포즈 불확실도에 따라 로그 오즈 점유 증거를 줄인다.
- **검증 목표:** 같은 스캔으로 만든 raw 지도보다 deskew 지도의 `x=4 m` 벽 평균 오차와 두께가 실제로 감소해야 한다.
- **중요한 경계:** 이 패키지는 알려진 TF를 쓰는 2D 교육 예제다. 위치 추정, 3D deskew, 동적 물체 제거, loop closure, hard real-time을 구현하거나 증명하지 않는다.

## 학습 순서 (Reading Order)

1. 아래 시스템 구조에서 센서 시각, TF 캐시, 지도 스냅샷의 경계를 찾는다.
2. `src/moving_scan_simulator.cpp`에서 `stamp+i×time_increment`마다 로봇 포즈가 달라지는 이유를 본다.
3. `src/motion_compensated_mapper.cpp`의 exact-time TF 조회와 `SE(2)` 보간 수식을 따라간다.
4. 같은 파일의 고정 배열·루프 상한·5 ms TF timeout이 보장하는 것과 보장하지 않는 것을 구분한다.
5. `src/map_quality_auditor.cpp`가 raw/deskew 지도를 독립적으로 비교하는 방법을 확인한다.
6. `paper_review.md`에서 LOAM이 왜 고주기 odometry와 저주기 mapping을 분리했는지 읽는다.
7. `knowledge/sensors/laser_scan_contract.md`, `knowledge/realtime/bounded_lidar_deskew.md`, `knowledge/mapping/motion_compensated_occupancy_mapping.md`로 개념을 복습한다.

## 시스템 구조

```mermaid
graph TD
  TRAJ[연속 SE2 궤적 모델] --> SIM[moving_scan_simulator]
  SIM -->|/tf 100 Hz<br/>map → base_link| TFB[(TF2 Buffer)]
  SIM -->|/tf_static<br/>base_link → laser| TFB
  SIM -->|/scan 10 Hz<br/>SensorDataQoS| MAP[motion_compensated_mapper]
  TFB -->|시작·끝 exact-time TF<br/>+ static extrinsic| MAP
  MAP -->|모든 광선을 시작 포즈에 투영| RAW[/mapping/raw]
  MAP -->|SE2 보간 + 신뢰도 가중| DESKEW[/mapping/deskewed]
  MAP -->|상한·지연·TF 통계| STATS[/mapping/stats]
  RAW --> AUDIT[map_quality_auditor]
  DESKEW --> AUDIT
  STATS --> AUDIT
  AUDIT -->|PASS / FAIL| RESULT[/mapping/audit]
```

더 큰 화면에서 시간·데이터 경계를 탐색하려면 `architecture.html`을 연다. 작성 내용은 한국어이며 고정 Viewer UI는 영어로 표시된다.

## 1. 기초 실무 — 센서 시각과 TF2 계약

`LaserScan.header.stamp`는 메시지를 publish한 시각이 아니라 **첫 광선의 취득 시각**이다. `i`번째 광선의 시각은 다음과 같이 복원한다.

```text
t_i = header.stamp + i × time_increment
theta_i = angle_min + i × angle_increment
```

라이다가 100 ms 동안 회전하며 스캔하면 `i=0`과 `i=180`은 서로 다른 로봇 포즈에서 측정된다. 모든 광선을 최신 포즈나 첫 포즈에 놓으면 벽이 휘거나 두꺼워진다. 이 실습은 `tf2_ros::Buffer::lookupTransform("map", "base_link", t)`로 스캔 시작과 끝의 과거 포즈를 요청한다. `TimePointZero`는 고정 `base_link→laser` 외부 파라미터의 최신 값을 읽는 데만 쓴다.

`SensorDataQoS`는 sensor stream의 작은 best-effort/volatile 큐다. 지도와 audit은 완전한 최신 스냅샷을 늦은 구독자도 받아야 하므로 `reliable().transient_local()`을 별도로 쓴다. 하나의 QoS를 모든 Topic에 기계적으로 적용하지 않는 이유다.

## 2. 심화·RT — bounded deskew hot path

광선마다 TF를 조회하면 181회의 캐시 탐색·락·예외 경로가 생긴다. 오늘 구조는 다음 상한을 명시한다.

```text
scan 당 TF 조회       = 시작 1 + 끝 1 + static extrinsic 1
scan 당 beam         <= 181
beam 당 ray sample   <= 100
TF 대기              <= 조회당 5 ms
내부 map storage     = 2 × 100 × 100 고정 배열
```

각 광선 포즈는 시작/끝 사이에서 보간한다.

```text
alpha_i = i / (N - 1)
p_i     = (1-alpha_i) p_start + alpha_i p_end
yaw_i   = yaw_start + alpha_i wrap(yaw_end - yaw_start)
```

`wrap`은 각도 차를 `[-π,π]`로 제한한다. 그렇지 않으면 `+179°→-179°`가 2°가 아니라 358° 회전으로 보간된다. 이 방식은 계산량이 고정이고 부드러운 짧은 구간에 유용하지만, 100 ms 안의 급격한 가속·미끄럼을 정확히 표현하지 못한다.

고정 배열과 루프 상한은 계산량/메모리의 상한을 명확히 할 뿐 hard RT 보장은 아니다. `tf2_ros::Buffer`, 로그, 메시지 `resize`, DDS publish, 일반 Linux scheduler는 여전히 동적 할당·락·비결정적 지연을 가질 수 있다. 제품에서는 callback을 non-RT ingest와 RT-safe math로 분리하고 trace/WCET/priority inversion을 측정해야 한다.

## 3. 알고리즘 — deskew와 불확실도 가중 로그 오즈

`map←base_link`와 `base_link←laser`의 `SE(2)` 합성은 다음과 같다.

```text
t_map_laser = t_map_base + R(yaw_base) t_base_laser
yaw_map_laser = yaw_map_base + yaw_base_laser
p_hit = t_map_laser + range × [cos(yaw+theta), sin(yaw+theta)]ᵀ
```

광선 내부 셀에는 free 로그 오즈, 실제 반사 끝점에는 occupied 로그 오즈를 더한다. deskew 지도는 끝점 위치 표준편차를 단순화해 계산한다.

```text
sigma_end² = sigma_xy² + (range × sigma_yaw)² + sigma_range²
w = 1 / (1 + (sigma_end / map_resolution)²)
l_cell ← clamp(l_cell + w × l_occupied, -4, 4)
```

`w`는 교육용 휴리스틱이다. 실제 센서 공분산을 엄밀히 격자 확률로 전파한 Bayesian inverse sensor model은 아니다. 그래도 “불확실한 점을 확실한 장애물처럼 강하게 찍지 않는다”는 설계 원리를 코드로 확인할 수 있다.

독립 auditor는 점유 확률 58 이상 셀을 모아 알려진 벽 `x=4 m`에 대한 가중 평균 절대 오차와 `±0.11 m` band 집중도를 계산한다. 30개 스캔 뒤 deskew 오차가 0.09 m보다 작고 raw의 80% 미만이며, band 집중도가 raw보다 0.10 이상 좋아야 PASS다. 30개를 기다리는 이유는 초기의 좁은 관측 각도만으로 raw/deskew 차이를 성급하게 판정하지 않기 위해서다.

## 빌드와 실행

```bash
source /opt/ros/jazzy/setup.bash
colcon build --base-paths daily_robotics/2026-09-21 \
  --build-base build/2026-09-21 \
  --install-base install/2026-09-21 \
  --event-handlers console_direct+
source install/2026-09-21/setup.bash
ros2 launch daily_robotics_2026_09_21 daily_demo.launch.py
```

별도 터미널에서 최종 독립 검증을 읽는다.

```bash
ros2 topic echo /mapping/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
```

결과에는 `PASS`, `raw_error_m`, `deskew_error_m`, `max_beams=181`, `max_steps<=100`이 포함되어야 한다. 이 PASS는 교육용 벽 시나리오와 명시된 처리 상한의 회귀 검증이지 실제 센서 정확도나 hard deadline의 증명이 아니다.

## 실무 확장 과제

1. 선형 보간을 wheel odometry/IMU의 constant-twist 또는 spline trajectory로 바꾸고 급가속 구간의 오차를 비교한다.
2. 3D `PointCloud2`의 per-point time/ring 필드를 파싱해 `SE(3)` deskew와 quaternion SLERP를 구현한다.
3. TF lookup과 map publish를 non-RT executor로 옮기고, preallocated beam math만 priority thread에서 실행한다.
4. 동적 물체, 유리, grazing angle에 따른 range-dependent inverse sensor model을 보정한다.
5. LOAM처럼 edge/plane feature residual로 odometry를 추정하되 loop closure가 없는 누적 drift를 별도 평가한다.

## 참고 자료

- [ROS 2 Jazzy `LaserScan` 원본 메시지](https://github.com/ros2/common_interfaces/blob/jazzy/sensor_msgs/msg/LaserScan.msg)
- [ROS 2 TF2 — Using time](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Tf2/Learning-About-Tf2-And-Time-Cpp.html)
- [ROS 2 TF2 — Traveling in time](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Tf2/Time-Travel-With-Tf2-Cpp.html)
- [ROS 2 Jazzy `SensorDataQoS`](https://docs.ros.org/en/jazzy/p/rclcpp/generated/classrclcpp_1_1SensorDataQoS.html)
- [ROS 2 Jazzy `OccupancyGrid`](https://docs.ros.org/en/jazzy/p/nav_msgs/msg/OccupancyGrid.html)
- [Zhang & Singh, LOAM: Lidar Odometry and Mapping in Real-time](https://www.ri.cmu.edu/publications/loam-lidar-odometry-and-mapping-in-real-time/)
- [LOAM open-access DOI](https://doi.org/10.15607/RSS.2014.X.007)
