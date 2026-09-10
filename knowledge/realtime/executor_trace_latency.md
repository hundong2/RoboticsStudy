# ROS 2 callback 지연 계측과 tracing

## 지연을 하나의 숫자로 부르지 말 것

Publisher가 메시지를 만든 뒤 subscriber callback이 끝날 때까지는 적어도 다음 구간이 있다.

1. publish/RMW serialization 및 transport
2. DDS receive queue 대기
3. executor wait-set wake-up과 callback dispatch 대기
4. 사용자 callback 실행
5. 후속 publish 및 middleware 처리

Application timestamp 차이는 여러 구간이 합쳐진 end-to-end 근사치다. `steady_clock`으로 callback 시작/끝을 재면 사용자 코드 실행시간을 볼 수 있지만, executor 대기 원인은 알 수 없다. 두 지표를 분리해 기록해야 “계산이 느린가, scheduling이 늦는가”를 구분할 수 있다.

## Hot-path 계측 원칙

- histogram bin과 max/counter는 `std::array`와 정수로 사전 할당한다.
- 계측 구간 안에서 문자열 formatting, logging, 파일 I/O를 피한다.
- ROS time은 simulation clock jump가 가능하므로 실행시간에는 monotonic `steady_clock`을 쓴다.
- 평균만 보지 말고 max, percentile/histogram, deadline miss를 본다.
- 계측 자체의 overhead를 baseline과 비교한다.

## Application 계측과 ros2_tracing의 역할

Application 계측은 도메인 단계와 성공/실패를 쉽게 연결하지만, executor/rcl/rmw 내부를 보지 못한다. Linux의 `ros2 trace`와 `tracetools` tracepoint는 callback start/end, executor, rcl/rmw 이벤트를 더 낮은 층에서 연결한다. 제품 성능 분석에서는 둘을 같은 재현 workload에서 함께 사용한다.

고정 buffer를 쓴다고 전체 node가 hard real-time이 되는 것은 아니다. DDS allocation, page fault, logger lock, allocator, kernel scheduling, CPU frequency와 IRQ affinity도 따로 통제·측정해야 한다.

## 참고

- [ROS 2 Jazzy — tracetools C++ API](https://docs.ros.org/en/jazzy/p/tracetools/generated/index.html)
- [ROS 2 Jazzy — CLI tools (`trace` 포함)](https://docs.ros.org/en/jazzy/Concepts/Basic/About-Command-Line-Tools.html)
