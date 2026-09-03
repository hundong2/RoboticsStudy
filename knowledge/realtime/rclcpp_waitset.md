# rclcpp WaitSet과 결정론적 실행

## 무엇인가

`rclcpp::WaitSet`은 subscription, timer, service, client, guard condition 같은 ROS 엔티티가 ready가 될 때까지 기다리는 저수준 API다. 일반 executor가 callback을 자동 선택하는 대신 애플리케이션이 다음을 결정한다.

- 어떤 엔티티를 함께 기다릴지
- ready 엔티티를 어느 순서로 `take`할지
- 여러 입력이 모두 모였을 때만 실행할지
- timeout 때 어떤 안전 동작을 할지

## 기본 패턴

```cpp
rclcpp::WaitSet wait_set;
wait_set.add_subscription(subscription);
while (rclcpp::ok()) {
  const auto result = wait_set.wait(std::chrono::milliseconds(10));
  if (result.kind() == rclcpp::WaitResultKind::Ready) {
    // ready index를 확인한 뒤 subscription->take(message, info)
  }
}
```

WaitSet에 등록한 subscription을 동시에 executor에도 등록하면 두 소비자가 같은 큐를 경쟁할 수 있으므로 피한다. 동적 엔티티 변경을 여러 스레드에서 수행한다면 synchronization policy도 검토해야 한다.

## RT 관점의 한계

WaitSet은 trigger와 처리 순서를 통제하는 도구이지 RT 인증 수단이 아니다. 결정론적 제어 루프에는 추가로 다음이 필요하다.

- PREEMPT_RT와 스레드 우선순위/CPU affinity
- page fault 방지를 위한 memory locking과 사전 touch
- 루프 내 heap allocation, blocking I/O, logging 제거
- RMW/DDS QoS, 직렬화, shared-memory 경로 검증
- `ros2_tracing` 등으로 WCET와 tail latency 측정

## 참고

- [ROS 2 Jazzy WaitSet examples](https://docs.ros.org/en/ros2_packages/jazzy/api/examples_rclcpp_wait_set/)
- [rclcpp WaitSetTemplate API](https://docs.ros.org/en/jazzy/p/rclcpp/generated/classrclcpp_1_1WaitSetTemplate.html)
- [Official ros2/examples source](https://github.com/ros2/examples/tree/jazzy/rclcpp/wait_set)
