# Intra-process 통신과 고정 용량 RT 경계

## 서로 다른 세 주장

다음 문장은 같은 뜻이 아니다.

1. 같은 프로세스다.
2. DDS serialization을 피했다.
3. callback 전체에서 동적 할당이 없다.

ROS 2 intra-process와 `std::unique_ptr` 단일 소유권 경로는 2번을 달성할 수 있지만, executor, logging, parameter service, 가변 길이 메시지 필드까지 자동으로 3번으로 만들지는 않는다.

## 할당을 줄이는 기본 패턴

- hot path 후보/상태는 `std::array<T, N>` 또는 사전 할당 pool 사용
- 후보 수, 반복 수, 입력 점 수의 상한을 상수로 문서화
- callback 안의 `std::vector::push_back`, 문자열 조합, 빈번한 INFO logging 제거
- 비 RT callback에서 검증/파싱하고 RT 경로에는 POD snapshot만 전달
- message 주소가 같다는 관찰과 end-to-end latency/WCET 증거를 분리

## `unique_ptr` 전달이 깨지는 흔한 경우

- 같은 Topic에 여러 intra-process subscriber가 서로 다른 소유권 형태를 요구함
- 한 노드에서 `use_intra_process_comms`가 꺼짐
- QoS 불일치로 intra-process 경로 자체가 연결되지 않음
- process 경계를 넘거나 network subscriber가 추가되어 inter-process 전달도 필요함

## 검증 체크리스트

```text
메시지 주소/trace → 복사 여부의 관찰
heap profiler       → callback 할당 횟수
page fault counter  → memory residency
callback histogram  → p50/p99/max 실행시간
scheduler trace     → priority inversion / preemption
```

“zero-copy”는 구성과 구현에 대한 측정 결과로 표현해야 하며, 코드에 `unique_ptr`가 보인다는 사실만으로 보장하지 않는다.

## 참고

- [ROS 2 Jazzy intra_process_demo](https://docs.ros.org/en/ros2_packages/jazzy/api/intra_process_demo/)
- [ROS 2 Executors](https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Executors.html)
