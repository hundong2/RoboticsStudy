# ROS 2 QoS Deadline & Liveliness — 누적 노트

## 서로 다른 두 계약

```text
Deadline   = "다음 샘플이 이 시간 안에 와야 한다"
Liveliness = "발행자가 이 lease 안에 살아 있음을 증명해야 한다"
```

Deadline 위반은 Publisher가 살아 있어도 센서 계산이 늦어졌다는 뜻일 수 있다. Liveliness 상실은 프로세스 정지, 통신 단절, heartbeat 실패처럼 발행자 생존을 의심할 신호다. 둘을 같이 쓰면 데이터 지연과 발행자 장애를 더 잘 구분할 수 있다.

## Manual-by-topic 패턴

```cpp
rclcpp::QoS qos(rclcpp::KeepLast(5));
qos.reliable()
   .deadline(100ms)
   .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
   .liveliness_lease_duration(250ms);

publisher->publish(message);
publisher->assert_liveliness();
```

Subscriber도 호환되는 Reliability, Deadline, Liveliness 정책을 요청해야 DDS endpoint가 연결된다. QoS event callback은 middleware 지원 범위에 따라 다를 수 있으므로 사용하는 RMW별 통합 시험이 필요하다.

## RT에서 오해하기 쉬운 점

- DDS Deadline은 callback의 완료시간이나 OS scheduler deadline을 보장하지 않는다.
- 이벤트 callback도 Executor가 실행하므로 Executor starvation이 있으면 알림 자체가 늦을 수 있다.
- callback 안에서 파일 I/O, 동기 Service, 대량 할당을 수행하지 않는다.
- event callback은 카운터/원자 플래그만 바꾸고, supervisor가 비 RT 경로에서 재시작·deactivate를 결정하게 한다.
- lease를 지나치게 짧게 하면 정상 jitter를 장애로 오탐한다. 센서 주기 분포의 p99와 네트워크 지연을 측정해 정한다.

## 권장 운영 지표

1. `offered/requested_deadline_missed.total_count_change`
2. `alive_count`, `not_alive_count`와 전이 시각
3. 메시지 `header.stamp` 기준 age와 inter-arrival histogram
4. Executor callback 시작 지연과 실행시간 p50/p95/p99/max
5. 장애 감지부터 안전 출력까지의 end-to-end 시간

## 연결 실습

- [`../../daily_robotics/2026-09-06/src/sensor_simulator.cpp`](../../daily_robotics/2026-09-06/src/sensor_simulator.cpp)
- [`../../daily_robotics/2026-09-06/src/lifecycle_ekf_node.cpp`](../../daily_robotics/2026-09-06/src/lifecycle_ekf_node.cpp)

## 레퍼런스

- [ROS 2 QoS settings](https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html)
- [rclcpp PublisherEventCallbacks](https://docs.ros.org/en/jazzy/p/rclcpp/generated/structrclcpp_1_1PublisherEventCallbacks.html)
