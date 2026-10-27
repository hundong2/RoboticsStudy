# Event Camera Packet 계약

## 대표 규칙

event camera의 한 event는 보통 `(x, y, t, polarity)`다. ROS 2 packet으로 묶을 때도 개별 event의 측정시각을 잃지 않아야 한다.

- `Header.stamp = t_ref`: packet의 명시적인 기준 시각
- `dt[i] = t_i - t_ref`: signed 상대시각; 단위와 범위를 interface에 고정
- `Header.frame_id`: `(x,y)`가 정의된 calibrated optical frame
- `count <= capacity`: fixed storage 중 실제 유효한 앞부분
- `polarity`: log-intensity 증가/감소 의미를 driver와 consumer가 동일하게 해석

## 왜 packet 수신시각은 측정시각이 아닌가

DDS callback에 도착한 시각은 driver batching, OS scheduling, transport jitter를 모두 포함한다. 이를 측정시각으로 쓰면 동일 장면을 실행할 때마다 motion compensation 결과가 달라질 수 있다. geometry와 sensor fusion에는 sensor clock에서 얻은 acquisition time을 사용하고, 수신시각은 latency telemetry로 따로 다룬다.

## 기준 시각 + offset 표현

event마다 64-bit 절대 timestamp를 반복하는 대신 기준 시각 하나와 정수 offset 배열을 둘 수 있다.

\[
t_i=t_{ref}+\Delta t_i
\]

장점은 wire/storage 크기를 줄이면서 packet 내부의 microsecond 순서를 보존하는 것이다. 단, offset 단위(`us`, `ns`), 부호, 허용 window, wraparound를 반드시 문서화해야 한다.

## QoS 체크리스트

- 고속 event stream: 최신성이 중요하면 `SensorDataQoS` 계열을 우선 검토
- calibration snapshot: 늦게 시작한 consumer가 받아야 하면 Reliable + Transient Local
- audit/result: 손실이 acceptance를 왜곡하면 Reliable
- publisher/subscriber reliability와 durability 호환성을 함께 확인

## 제품 경계

fixed application message가 DDS/RMW 내부의 모든 allocation을 제거하는 것은 아니다. 실제 zero-copy/loaned-message 지원, serialization, driver buffer ownership은 사용하는 RMW와 message type에서 별도 검증한다. 또한 sensor clock과 ROS clock offset/drift 보정, hot-pixel/background-activity filter, packet overflow/drop counter를 운영 telemetry에 포함한다.

## 참고

- [ROS 2 QoS settings](https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html)
- [ROS 2 custom interfaces](https://docs.ros.org/en/jazzy/Tutorials/Beginner-Client-Libraries/Custom-ROS2-Interfaces.html)
- [ROS 2 `std_msgs/Header`](https://docs.ros.org/en/jazzy/p/std_msgs/msg/Header.html)
