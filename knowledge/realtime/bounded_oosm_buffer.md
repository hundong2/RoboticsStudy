# Bounded OOSM 상태 버퍼

OOSM rewind/replay는 지연 측정의 정확도를 회복하지만 과거 전체를 저장·재계산하면 메모리와 실행시간이 무한히 늘 수 있다. RT 경계에서는 고정 용량 `C`의 snapshot ring과 replay 상한을 먼저 정한다.

- 메모리 상한: `C * sizeof(Snapshot)`으로 정적 계산
- 시간 horizon: 대략 `C / input_rate`; 실제 판단은 첫/마지막 stamp로 수행
- 연산 상한: 한 측정당 최대 `C-1`번 predict/update
- 초과 정책: horizon 밖 측정을 drop하고 counter/age를 진단으로 공개
- 계측: replay step 수와 `steady_clock` 실행시간의 max/histogram을 별도 기록

`std::array`와 bounded loop는 수치 kernel의 상한만 만든다. ROS message 복사, allocator, DDS queue, executor wakeup, OS scheduling은 별도의 지연 원인이다. hard real-time을 주장하려면 allocator/transport/scheduler를 포함한 WCET 분석과 deadline fault handling이 필요하다.

Capacity는 평균 delay가 아니라 `최대 지연 + jitter + executor backlog + margin`을 input period로 나눈 값에서 출발한다. 큰 capacity는 오래된 측정을 더 살리지만 한 번의 replay 비용과 stale information risk도 늘린다.
