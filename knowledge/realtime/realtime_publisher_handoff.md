# RealtimePublisher와 RT/Non-RT Handoff

## 왜 일반 publish를 update()에서 피하는가

일반 ROS publish 경로는 middleware queue, serialization, allocator, lock 같은 실행시간 변동을 포함할 수 있다. `realtime_tools::RealtimePublisher`는 RT loop가 미리 할당된 message를 채우고, 별도 non-RT thread가 실제 ROS publish를 수행하도록 책임을 나눈다.

```text
RT update --trylock--> preallocated message --unlockAndPublish--> non-RT publisher thread --> DDS
          \--lock 실패: 기다리지 않고 sample drop + miss counter
```

핵심은 `trylock()` 실패 시 기다리지 않는 것이다. 진단은 유실 가능하지만 control deadline은 진단보다 중요하다. message의 `vector`/`string`은 `on_configure()`에서 최대 크기로 준비하고 update에서는 resize·push_back·문자열 조립을 하지 않는다.

## RealtimeBuffer

Subscriber callback은 non-RT executor에서 `writeFromNonRT()`로 고정 크기 명령을 쓰고 controller `update()`는 `readFromRT()`로 최신 snapshot을 읽는다. RT reader는 mutex를 얻지 못해도 이전 snapshot을 반환하므로 기다리지 않는다. 단, 쓰는 자료형 자체가 복사 중 allocation을 일으키지 않도록 fixed-size POD를 선호한다.

## 검증 항목

- publish miss와 interface-access miss를 누적 counter로 노출한다.
- controller update rate보다 진단 rate를 낮춘다.
- RT loop에 `new`, vector resize, logger, blocking lock, filesystem/network I/O가 없는지 정적 검토한다.
- target CPU에서 page fault, CPU isolation, IRQ affinity, SCHED_FIFO, bus latency와 WCET/tail latency를 측정한다.
- subscriber가 느리거나 사라진 조건에서도 control loop deadline이 유지되는지 시험한다.

## 공식 자료

- [Jazzy RealtimePublisher API](https://control.ros.org/jazzy/doc/api/classrealtime__tools_1_1RealtimePublisher.html)
- [ros2_control Controller Manager RT guidance](https://control.ros.org/jazzy/doc/ros2_control/controller_manager/doc/userdoc.html)
