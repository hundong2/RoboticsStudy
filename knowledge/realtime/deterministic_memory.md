# ROS 2 결정론적 메모리와 Loaned Message

## 평균이 빠른 것과 상한이 있는 것은 다르다

`new`, `malloc`, `std::vector` 재할당은 대부분 빠르지만 allocator lock, heap fragmentation, page fault 때문에 최악 실행시간이 튈 수 있다. hard real-time 경로에서는 다음을 설계 시점에 정한다.

- 최대 메시지/큐/노드 수
- 초기화 후 추가 할당 금지 범위
- overflow 시 drop/overwrite/fail 중 정책
- page prefault와 `mlockall` 적용 여부
- deadline 초과 시 안전 동작

## bounded container 패턴

```text
초기화 구간: 메모리 확보, DDS entity 생성, TF/cache 준비
RT 구간:     고정 배열/풀에서 읽기·쓰기, blocking과 logging 금지
비-RT 구간:  가변 길이 Path 생성, 파일/네트워크 I/O, 진단 문자열
```

`std::array<T,N>`은 저장 크기가 컴파일 시 고정된다. 그러나 배열을 쓰는 함수가 logging, TF lookup, DDS publish를 호출한다면 함수 전체가 자동으로 RT-safe해지는 것은 아니다. RT core와 integration shell을 분리해 측정해야 한다.

## Loaned message가 하는 일

일반 publish 경로는 애플리케이션 메시지를 RMW/DDS 내부 버퍼로 복사하거나 직렬화하면서 메모리를 추가로 잡을 수 있다. loaned message는 middleware가 관리하는 메시지 저장소를 빌려 애플리케이션이 직접 채운 뒤 소유권을 반환한다.

```cpp
if (publisher->can_loan_messages()) {
  auto loan = publisher->borrow_loaned_message();
  loan.get().data = value;
  publisher->publish(std::move(loan));
}
```

장점 후보:

- 애플리케이션→middleware 복사 감소
- 사전 할당 pool을 쓰는 RMW에서 allocator jitter 감소
- shared-memory transport와 결합 시 큰 메시지의 복사 감소

## 보장하지 않는 것

- 모든 RMW가 loan을 지원하지 않는다.
- unbounded string/vector가 든 메시지는 zero-copy가 제한될 수 있다.
- inter-process, intra-process, network transport의 복사 경로가 서로 다르다.
- loan을 썼다는 사실만으로 scheduler, page fault, interrupt latency가 사라지지 않는다.
- publish 뒤 이동된 loan 객체나 메시지 참조를 다시 사용하면 lifetime 오류가 난다.

따라서 시작 시 `can_loan_messages()`를 기록하고, fallback 경로도 기능 테스트한다. RMW별 tracing과 최대 latency를 측정해 “zero-copy”라는 설정 이름이 아니라 실제 경로를 검증한다.

## 실무 체크리스트

- [ ] 메시지 타입의 string/sequence가 bounded인지 확인
- [ ] publisher/subscriber/RMW가 각각 loan을 지원하는지 확인
- [ ] loan 성공 경로와 일반 publish fallback을 모두 테스트
- [ ] 빌드 후 allocation counter 또는 tracing으로 steady state 할당 측정
- [ ] 평균, p99만이 아니라 worst-case와 deadline miss 수 기록
- [ ] RT thread에서 logging, TF lookup, parameter service, 파일 I/O 분리

## 참고

- [ROS 2 zero-copy via loaned messages design](https://design.ros2.org/articles/zero_copy.html)
- [ROS 2 real-time systems proposal](https://design.ros2.org/articles/realtime_proposal.html)
