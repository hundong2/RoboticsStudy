# Bounded Grid Processing for Real-Time Robotics

## Bounded의 의미

"평균 100 us"와 "최악에도 정해진 연산량"은 다르다. 실시간 설계에서는 입력 크기, queue 용량, 반복 횟수, 실패 동작을 계약해야 한다.

예를 들어 `W≤32`, `H≤32`, `N=W×H≤1024`이고 각 cell을 고정 횟수로 방문하면 알고리즘 연산량의 상한을 설명할 수 있다. 계약보다 큰 입력은 buffer에 억지로 넣지 말고 즉시 거부하거나 비-RT 경로로 넘긴다.

## 패턴

- hot path 배열은 `std::array<T, Max>`처럼 시작 전에 용량을 확정한다.
- BFS/DFS queue도 최대 cell 수로 고정하고, 각 cell이 한 번만 enqueue된다는 invariant를 검증한다.
- 재귀 flood fill은 call stack 깊이가 data에 따라 바뀌므로 fixed queue 반복으로 바꾼다.
- callback 실행시간은 wall clock이 아니라 `steady_clock`으로 측정한다.
- 로그, 문자열 formatting, visualization message는 계측 구간 밖의 저주기 경로로 분리한다.
- budget miss counter와 worst observed time을 함께 남긴다.

## 보장하지 않는 것

알고리즘 loop가 bounded여도 ROS message의 `std::vector`/`std::string`, DDS serialization, page fault, mutex, executor와 OS scheduler는 별도 jitter를 만든다. hard RT를 주장하려면 다음도 확인해야 한다.

1. 시작 전 memory prefault와 `mlockall`
2. RT thread priority/affinity와 priority inversion
3. allocator/RMW의 worst-case 특성
4. callback chain의 end-to-end latency
5. target hardware의 장시간 stress test와 tail percentile가 아닌 실제 maximum 추적

## 점진 처리 대안

큰 지도를 한 callback에서 전부 처리할 수 없다면 매 tick의 cell quota를 정하고 state machine으로 여러 주기에 나눈다. 이때 map version을 붙여 중간 계산 중 새 지도가 오면 안전하게 취소/재시작해야 한다.
