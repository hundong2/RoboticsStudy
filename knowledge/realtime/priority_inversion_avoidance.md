# Priority Inversion 회피와 ROS 2 Callback 격리

## 문제 형태

`Low` thread가 mutex를 가진 상태에서 `High`가 같은 mutex를 요청하면 High가 막힌다. 이때 mutex와 무관한 `Medium`이 Low를 선점하면, High는 Medium보다 우선순위가 높은데도 더 늦게 끝난다. 이것이 unbounded priority inversion의 전형이다.

## 대응 순서

1. **공유를 제거한다:** high callback이 log/parameter/planning-scene mutex를 읽지 않게 snapshot/message passing 경계를 둔다.
2. **실행을 격리한다:** 서로 다른 callback group을 다른 executor/thread에 배치하고 OS priority/affinity를 명시한다.
3. **handoff를 bounded하게 만든다:** fixed-capacity SPSC queue, atomic snapshot, sequence counter처럼 retry와 용량에 상한을 둔다.
4. **공유가 불가피하면 protocol을 쓴다:** `PTHREAD_PRIO_INHERIT` 또는 priority ceiling을 검토하되 critical section을 짧게 유지한다.
5. **측정한다:** callback execution time, queue latency, timer jitter, page fault, scheduler switch를 분리해 관찰한다.

## ROS 2 주의점

- `MultiThreadedExecutor`를 쓴다고 자동으로 callback이 분리되지 않는다. 기본 callback group 하나만 쓰면 사실상 직렬화될 수 있다.
- callback group은 동시 실행 규칙이지 OS thread priority 자체가 아니다. executor/thread 배치와 scheduler 설정이 추가로 필요하다.
- lock-free application handoff여도 `rclcpp`, RMW, DDS, allocator, logging이 hard-RT-safe라는 뜻은 아니다.
- 원자 타입의 `is_lock_free()`는 target platform에서 확인한다.

참고: [ROS 2 Jazzy Callback Groups](https://docs.ros.org/en/jazzy/How-To-Guides/Using-callback-groups.html), [callback-group-level executor 예제](https://docs.ros.org/en/jazzy/p/examples_rclcpp_cbg_executor/)
