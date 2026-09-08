# ROS 2 제어 callback의 시간 예산 측정

## 측정할 두 값

**Callback 실행시간(execution time)** 은 callback 시작부터 종료까지 걸린 시간이다.

**시작 jitter** 는 이상적인 시작 시각과 실제 시작 시각의 차이다.

5 ms 주기의 callback이 100 us 안에 계산을 끝내도 scheduler나 앞 callback 때문에 4 ms 늦게 시작하면 actuator 명령은 이미 늦다. 두 값을 반드시 분리해 기록해야 한다.

## 최소 계측 패턴

```cpp
auto start = std::chrono::steady_clock::now();
run_control();
auto finish = std::chrono::steady_clock::now();

execution = finish - start;
jitter = abs(start - expected_start);
expected_start += period;
```

`steady_clock`은 wall-clock 보정으로 뒤로 가지 않아 duration 측정에 적합하다. `expected_start = start + period`로 갱신하면 지연이 누적될 때 drift를 숨기므로, 이상적 timeline에 `period`를 더한다.

## RT 친화적 통계

고주기 callback에서 매 표본을 `vector`나 log 파일에 추가하면 allocator와 I/O가 측정을 오염시킨다.

- 고정 크기 histogram bin을 사용한다.
- `max`, sample count, miss count만 정수로 누적한다.
- 문자열 formatting과 ROS logging은 저주기 진단 callback으로 보낸다.
- 가능하면 tracepoint/LTTng처럼 시스템 수준 도구와 교차 검증한다.

## “고정 계산량”이 뜻하는 것

고정 horizon, 고정 solver iteration, `std::array`는 경로를 이해하기 쉽게 하지만 hard real-time 인증을 자동으로 주지 않는다.

여전히 확인할 것:

- Publisher/RMW가 호출 중 할당하거나 block하는가?
- page fault를 방지하도록 memory locking과 prefault를 했는가?
- thread policy/priority/CPU affinity가 설정되었는가?
- priority inversion 가능한 mutex가 있는가?
- DDS receive thread와 IRQ가 어느 CPU/priority에서 실행되는가?
- worst-case cache miss와 thermal throttling에서 deadline을 만족하는가?

`daily_robotics/2026-09-09`의 histogram은 관찰 장치이지 RT 보증서가 아니다.
