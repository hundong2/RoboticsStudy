# PREEMPT_RT와 ROS 2 제어 루프 경계

## 핵심 관점

실시간 시스템의 목표는 평균이 빠른 것이 아니라 **deadline까지 끝나는 시간의 상한과 deadline miss 대응을 설계하는 것**이다. PREEMPT_RT 커널, 스케줄링 정책, 메모리, 애플리케이션 코드, RMW/DDS 경로를 함께 봐야 한다.

## Linux 측 구성 요소

- PREEMPT_RT: 많은 커널 잠금을 priority-inheritance 가능한 `rtmutex`로 바꾸고 인터럽트를 스레드화해 선점 가능 구간을 늘린다.
- `SCHED_FIFO`: 더 높은 우선순위 runnable 스레드가 일반 스레드를 선점한다. 잘못 설계된 무한 루프는 시스템을 굶길 수 있다.
- `mlockall`: 메모리 페이지를 RAM에 고정해 제어 중 page fault 가능성을 줄인다.
- CPU affinity/IRQ affinity: 제어 스레드와 방해 요인을 CPU 코어 수준에서 분리할 때 사용한다.
- RT throttling과 권한: `CAP_SYS_NICE`, `RLIMIT_RTPRIO`, `RLIMIT_MEMLOCK` 및 컨테이너 설정을 함께 확인한다.

## 제어 루프 패턴

```text
non-RT executor
  ├─ Goal/parameter/log/DDS callbacks
  └─ atomic or bounded queue handoff
                  ↓
RT control thread
  ├─ absolute deadline sleep
  ├─ bounded sensor snapshot
  ├─ fixed-cost control calculation
  └─ bounded command handoff
```

상대 sleep을 매번 호출하면 한 주기의 지연이 이후 주기에 누적된다. `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)`처럼 절대 deadline을 사용하면 장기 drift를 줄일 수 있다.

## RT 루프에서 피할 것

- 동적 메모리 할당과 크기가 입력에 따라 무제한 증가하는 컨테이너
- page fault, 파일/네트워크 I/O, DNS
- 무제한 mutex 대기와 priority inversion
- 형식화 로그와 예외 경로
- 입력 크기에 따라 반복 횟수가 무제한인 알고리즘
- WCET를 측정하지 않은 DDS publish/service/action 호출

## 측정

1. 호스트의 PREEMPT_RT 커널 여부와 스레드 정책/우선순위를 기록한다.
2. `cyclictest`로 OS scheduling latency baseline을 측정한다.
3. 실제 애플리케이션에 wake-up, compute-finish, publish timestamp를 넣는다.
4. 정상/CPU/메모리/I/O/네트워크 부하에서 p50/p99/p99.9/max를 비교한다.
5. deadline miss 시 zero command, hold, degraded mode, watchdog 중 안전 동작을 검증한다.

## 관련 실습

- `daily_robotics/2026-09-02`: callback group, executor, bounded SPSC queue
- `daily_robotics/2026-09-03`: bounded memory와 loaned-message fallback
- `daily_robotics/2026-09-04`: WaitSet 기반 readiness/take 순서
- `daily_robotics/2026-09-05`: `SCHED_FIFO`, `mlockall`, absolute deadline, Action/RT atomic handoff

## 참고 자료

- [Linux kernel PREEMPT_RT theory](https://docs.kernel.org/core-api/real-time/theory.html)
- [ROS 2 real-time systems proposal](https://design.ros2.org/articles/realtime_proposal.html)
