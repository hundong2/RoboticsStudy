# ROS 2 Callback Group 우선순위 격리

## 문제

`MultiThreadedExecutor`는 callback 병렬 실행을 허용하지만 어떤 OS thread가 어떤 callback을 실행할지, 높은 중요도의 callback이 먼저 실행될지를 자동으로 보장하지 않는다. 센서 serialization, logging, service callback이 motor update와 같은 worker pool을 공유하면 tail latency와 priority inversion을 분석하기 어렵다.

## 기본 패턴

```text
Node
├─ control_group → control_executor → 전용 RT thread
└─ io_group      → io_executor      → 일반 thread

I/O callback ── atomic snapshot ──> control callback
control callback ── atomic snapshot ──> telemetry callback
```

1. `create_callback_group(MutuallyExclusive)`로 control/I/O group을 나눈다.
2. Node 전체를 executor에 추가하지 않고 `add_callback_group()`으로 각 group을 별도 executor에 등록한다.
3. control executor를 전용 thread에서 spin한다.
4. Linux에서 검증된 우선순위·affinity를 그 thread에 적용한다.
5. 그룹 사이에는 bounded, non-blocking, ownership이 명확한 buffer만 둔다.

## Atomic snapshot 주의점

관련 필드 여러 개는 한 transaction으로 읽혀야 한다. 각 필드가 atomic이라는 사실만으로 snapshot 일관성이 생기지 않는다. sequence를 앞뒤로 검사하는 방식은 다음 조건을 확인해야 한다.

- payload 자체도 atomic이어서 C++ data race가 없어야 한다.
- writer가 하나이거나 writer 직렬화 계약이 있어야 한다.
- `is_lock_free()`를 실제 target CPU/toolchain에서 확인해야 한다.
- reader retry 상한과 writer 폭주 가능성을 WCET 분석에 포함해야 한다.
- 복잡한 payload는 검증된 `realtime_tools::RealtimeBuffer`나 bounded SPSC 구조를 우선 검토한다.

## RT라고 주장하기 위한 증거

- `SCHED_FIFO`/`SCHED_RR` 설정 성공과 실제 thread policy/priority
- CPU affinity, isolation, IRQ placement
- memory prefault/lock과 hot path allocation trace
- callback 주기 histogram, p99.9/max latency, deadline miss
- lock contention, RMW publish/take 비용, logging 부재
- overload와 runaway thread 시 watchdog/failsafe 동작

권한 없는 container에서 `pthread_setschedparam` 실패 후 기능이 계속되는 것은 좋은 개발 fallback일 수 있지만 RT 성공 증거는 아니다.

## Deadlock과 생명주기

동기 service/action을 callback 안에서 기다리면 hidden done-callback과 같은 `MutuallyExclusive` group을 공유해 deadlock이 생길 수 있다. 비동기 API를 쓰거나 대기 callback과 완료 callback을 서로 다른 group에 둔다. 종료 시 두 executor가 동일 ROS context의 shutdown을 관찰하고 thread가 join되는지도 시험한다.

- [ROS 2 Jazzy: Using Callback Groups](https://docs.ros.org/en/jazzy/How-To-Guides/Using-callback-groups.html)
- [ROS 2: Executors](https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Executors.html)
