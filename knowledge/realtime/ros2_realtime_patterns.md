# ROS 2 Real-Time 패턴 — 누적 노트

## Real-time의 기준

Real-time은 “평균적으로 빠름”이 아니라 **정해진 deadline 안에 계산이 끝난다는 시간적 정확성**이다. 따라서 평균/중앙값보다 worst-case latency, jitter, deadline miss 비율을 먼저 본다.

```text
end-to-end latency
= sensor/driver + DDS transport + executor wait
+ callback WCET + scheduling interference + actuator/driver
```

## RT 경로에서 피할 것

- 크기가 정해지지 않은 `new/delete`, 컨테이너 성장, 문자열 조합
- 디스크 I/O, 동기 네트워크 호출, 콘솔 출력
- 상한 없는 mutex 대기와 priority inversion
- page fault, swap, 처음 실행되는 lazy initialization
- 실행 중 thread/process 생성

## 설계 패턴

### 준비 / 주기 실행 / 정리 분리

노드 시작 시 메모리, thread, DDS endpoint를 준비한다. 주기 실행 구간은 고정 크기 자료구조와 bounded 연산만 사용하고, 로깅/파일 저장은 낮은 우선순위 경로로 넘긴다.

### Callback group으로 간섭 경계 표현

- `MutuallyExclusive`: 같은 그룹 콜백은 동시에 실행하지 않는다. 공유 상태 직렬화에 유용하다.
- `Reentrant`: 같은 콜백도 동시 실행 가능하다. 구현이 실제로 thread-safe할 때만 쓴다.
- 서로 다른 그룹은 multi-threaded executor에서 병렬 실행될 수 있다.

그룹을 나눈 것만으로 우선순위가 생기지는 않는다. 엄격한 제어 주기라면 그룹을 별도 executor/thread에 배치하고 OS scheduling priority와 CPU affinity를 설계한다.

### Bounded queue와 backpressure

생산 속도가 소비 속도를 영원히 넘을 수 있다면 유한 메모리 시스템에는 반드시 정책이 필요하다.

- 최신성 우선 센서: 오래된 샘플 또는 새 샘플 drop
- 모든 이벤트 보존: upstream 속도 제한, 저장소 용량 산정, 명시적 overload 상태
- 제어 명령: sequence/deadline 검사와 fail-safe 상태 전이

Lock-free는 “항상 빠르다”가 아니라 한 thread의 정지가 다른 thread의 mutex 획득을 막지 않는 진행 보장 방식이다. SPSC 큐는 정확히 한 producer와 한 consumer일 때만 단순한 구현이 안전하다.

## Linux 검증 체크리스트

1. PREEMPT_RT kernel과 IRQ/thread 우선순위 정책 확인
2. `cyclictest`를 실제 CPU/열/네트워크 부하와 함께 실행
3. `mlockall(MCL_CURRENT | MCL_FUTURE)`와 stack prefault 검토
4. RT thread의 `SCHED_FIFO`/`SCHED_RR` 우선순위, CPU affinity 설정
5. RT thread가 사용하는 CPU에서 불필요한 IRQ와 일반 worker 이동
6. `ros2_tracing`, LTTng, perf로 callback/스케줄링 구간 측정
7. page fault, context switch, queue drop, deadline miss를 장시간 기록

관리자 권한으로 우선순위를 올리기 전에 watchdog과 안전한 종료 경로를 준비한다. 높은 우선순위의 무한 루프는 시스템 전체를 굶길 수 있다.

## 중요한 한계

- `std::atomic::is_always_lock_free` 여부는 타입/플랫폼마다 다르다.
- ROS 메시지 publish/subscribe 내부 할당과 복사는 RMW 및 전송 방식에 따라 달라진다.
- loaned message와 shared-memory transport 지원은 배포 대상 조합에서 직접 검증한다.
- PREEMPT_RT만 설치해도 사용자 코드의 blocking/allocation이 사라지지는 않는다.

## 참고

- [ROS 2: Introduction to Real-time Systems](https://design.ros2.org/articles/realtime_background.html)
- [ROS 2 Executors](https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Executors.html)
- [ROS 2 QoS settings](https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Quality-of-Service-Settings.html)
