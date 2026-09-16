# 고정 용량 센서 재생 파이프라인

센서 콜백 경로를 `std::array` 고정 큐에 수치 복사/색인만 하는 O(1) 작업으로 제한하고, 별도 처리 루프가 주기마다 최대 B개를 꺼내면 적분 알고리즘의 **한 번의 작업량**을 상한 지을 수 있다. 도착률 λ와 처리 가능률 μ는 평균뿐 아니라 순간 burst까지 비교해야 한다. 큐 용량 C와 버스트/스케줄 지연이 맞지 않으면 overflow를 감추지 말고 계수하고 재설정·드롭 정책을 정의한다.

단일 스레드 executor에서는 enqueue와 drain 콜백이 동시에 접근하지 않으므로 이 예제는 잠금이 없다. 다중 executor/스레드로 바꾸면 이 전제가 깨지므로 SPSC 소유권, 원자 연산 또는 RT-safe handoff를 새로 설계해야 한다. `publish()`의 DDS 경로, 메모리 할당, OS wakeup, 로그는 고정 배열과 별도로 계측해야 한다. bounded loop는 hard real-time 인증과 같지 않다.

오늘의 구체적 계약: C=256, B=16, 20 ms 배치, 100 Hz 입력, 4초마다 timestamp 역행. `max_batch`, overflow, reset을 공개하고 해석해와 비교한다. 더 일반적인 callback budget 및 RT 분리는 [callback_budget_measurement.md](callback_budget_measurement.md), [ros2_realtime_patterns.md](ros2_realtime_patterns.md)를 참조한다.
