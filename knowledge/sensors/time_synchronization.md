# 다중 센서 시간 동기화

## 세 가지 시간을 구분한다

센서 메시지에는 적어도 다음 시간이 섞일 수 있다.

1. **측정 시각:** 광자가 노출되거나 LiDAR beam이 발사된 hardware time
2. **Driver stamp 시각:** Driver가 측정값에 `Header.stamp`를 쓴 시각
3. **도착/처리 시각:** DDS가 수신하고 executor가 callback을 시작한 시각

Arrival이 가깝다고 측정 시각도 가까운 것은 아니다. Network batching, driver queue, executor 지연 때문에 순서와 간격이 달라질 수 있다. Sensor fusion/SLAM에서는 가능하면 같은 hardware clock domain의 측정 시각을 사용한다.

## ExactTime과 ApproximateTime

- `ExactTime`: stamp가 정확히 같은 메시지만 짝짓는다. Hardware trigger와 공통 stamp 정책이 있을 때 명확하다.
- `ApproximateTime`: queue 안의 stamp를 비교해 가까운 후보를 선택한다. 독립 센서 rate와 작은 jitter를 흡수하지만 잘못된 pair 가능성과 추가 대기 지연이 생긴다.

Queue size와 age penalty는 선택 정책이다. 안전/품질 계약은 callback에서 `|t_a-t_b|≤Δt_max`를 다시 검사하고, 초과 pair를 reject/fault 처리해야 한다.

## 공간 오차로 환산하기

시간 오차 `Δt`는 로봇 속도 `v`와 각속도 `ω`에서 대략

\[
e_{position}\approx |v|\Delta t,\qquad
e_{angle}\approx |\omega|\Delta t
\]

의 motion distortion를 만든다. 예를 들어 2 m/s에서 20 ms는 4 cm다. 센서 noise보다 큰 시간 유래 오차를 알고리즘 outlier threshold로 덮으면 calibration/clock 문제를 숨길 수 있다.

## 실무 체크리스트

- PTP/IEEE 1588, hardware trigger, PPS 등 clock 동기 방식과 holdover 조건 기록
- Camera exposure mid-time, LiDAR scan start/end 중 무엇을 stamp로 쓰는지 확인
- `frame_id`와 extrinsic calibration version을 데이터와 함께 추적
- Queue overflow/drop 정책과 최대 허용 age 명시
- `/use_sim_time`, rosbag replay rate, clock jump 시험
- Arrival time 대신 Header stamp 기반 latency와 pair skew를 각각 계측
- 센서 rate가 다를 때 한 sample의 재사용 허용 여부를 명시

## 참고

- [ROS 2 Jazzy message_filters — Approximate Time Synchronizer](https://docs.ros.org/en/ros2_packages/jazzy/api/message_filters/doc/Tutorials/Approximate-Synchronizer-Cpp.html)
