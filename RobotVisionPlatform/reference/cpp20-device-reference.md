# C++20 장치 코드 사전

## `class`와 interface

`ICamera`, `IDetector`, `IEventSink`는 구현이 반드시 제공해야 할 함수만 선언하는 interface 역할입니다.

```cpp
class IDetector {
 public:
  virtual ~IDetector() = default;
  virtual std::vector<Detection> Infer(const Frame& frame) = 0;
};
```

- `virtual`: 실제 객체의 override 함수를 호출하게 합니다.
- `= 0`: 구현이 없는 pure virtual 함수라는 뜻입니다.
- virtual destructor: interface pointer로 삭제해도 concrete destructor가 실행됩니다.
- 위치: [`interfaces.hpp`](../device/include/rv/interfaces.hpp)

새 detector는 다음 형태입니다.

```cpp
class TensorRtDetector final : public rv::IDetector {
 public:
  std::vector<rv::Detection> Infer(const rv::Frame& frame) override;
  std::string Version() const override { return "person-detector/1.0.0"; }
};
```

`final`은 더 이상 상속하지 않음을, `override`는 base 함수와 signature가 정확히 같은지 compiler가 검사함을 뜻합니다.

## `struct`와 aggregate initialization

단순 데이터 묶음은 `struct`로 선언하고 지정 초기화할 수 있습니다.

```cpp
rv::Detection detection{
    .label = "person",
    .confidence = 0.92F,
    .x = 0.1F,
    .y = 0.2F,
    .width = 0.3F,
    .height = 0.5F};
```

- 멤버는 선언된 순서를 지켜 지정합니다.
- `0.92F`의 `F`는 float literal임을 나타냅니다.
- 위치: [`types.hpp`](../device/include/rv/types.hpp)

## `std::unique_ptr<T>`

객체 소유자가 한 곳뿐임을 나타내는 smart pointer입니다.

```cpp
auto detector = std::make_unique<TensorRtDetector>();
std::unique_ptr<rv::IDetector> base = std::move(detector);
```

- `std::make_unique`: heap 객체 생성과 pointer wrapping을 한 번에 합니다.
- `std::move`: 소유권을 복사하지 않고 이전합니다. 이전 pointer는 비게 됩니다.
- Pipeline이 adapter 소유권을 가지므로 raw `new/delete`가 필요 없습니다.
- 위치: [`pipeline.hpp`](../device/include/rv/pipeline.hpp)

## `const T&`

복사 없이 읽기 전용으로 전달하는 reference입니다.

```cpp
std::vector<Detection> Infer(const Frame& frame);
```

- `Frame` 전체 pixel buffer를 복사하지 않습니다.
- 함수가 반환되기 전까지만 `frame`을 사용해야 합니다.
- `const`이므로 함수 내부에서 frame을 수정할 수 없습니다.

## `std::jthread`

C++20의 자동 join thread입니다.

```cpp
worker = std::jthread([this](std::stop_token stop) {
  WorkLoop(stop);
});
```

- 생성 즉시 lambda를 새 thread에서 실행합니다.
- destructor는 종료 요청 후 join합니다.
- 프로젝트는 명시적 `Stop()`으로 종료 순서를 분명하게 합니다.
- 위치: [`pipeline.cpp`](../device/src/pipeline.cpp)

## `std::stop_token`과 `request_stop()`

worker에게 강제 종료 대신 협력적 종료를 요청합니다.

```cpp
worker.request_stop();
while (!stop.stop_requested()) {
  // 한 단위 작업
}
```

stop 요청은 exception이나 thread kill이 아닙니다. loop와 blocking wait가 token을 확인해야 종료됩니다.

## `std::condition_variable_any::wait`

queue가 비어 있을 때 CPU를 소비하는 반복 확인 대신 thread를 잠재웁니다.

```cpp
ready.wait(lock, stop, [this] { return closed || !items.empty(); });
```

- `lock`: 기다리는 동안 mutex를 풀고, 깨어날 때 다시 잡습니다.
- `stop`: 종료 요청도 wake-up 조건입니다.
- predicate: spurious wake-up이 생겨도 실제 조건을 다시 검사합니다.
- 위치: [`bounded_queue.hpp`](../device/include/rv/bounded_queue.hpp)

## `std::scoped_lock`과 `std::unique_lock`

```cpp
std::scoped_lock lock(mutex);  // scope 끝에서 자동 unlock
std::unique_lock lock(mutex);  // wait처럼 unlock/relock이 필요한 경우
```

직접 `mutex.lock()`/`unlock()`을 호출하면 중간 exception이나 return에서 unlock을 잊을 수 있습니다.

## `std::optional<T>`

“값이 있음”과 “정상적으로 값이 없음”을 모두 표현합니다.

```cpp
auto value = queue.Pop(stop);
if (!value) break;
Use(*value);
```

이 프로젝트에서 `nullopt`는 queue close 또는 stop으로 consumer가 끝나야 함을 뜻합니다.

## `std::atomic_uint64_t`

여러 thread가 counter를 수정할 때 data race를 막습니다.

```cpp
std::atomic_uint64_t captured{};
++captured;
auto snapshot = captured.load();
```

여러 필드가 반드시 같은 순간의 값이어야 한다면 atomic 여러 개만으로는 부족하고 별도 lock/snapshot 설계가 필요합니다.

## `std::move`

큰 buffer 또는 vector의 내부 자원을 새 객체로 이전합니다.

```cpp
frames.Push(std::move(frame));
event.detections = std::move(detections);
```

move 이후 원본은 유효하지만 내용은 보장되지 않으므로 다시 읽지 않습니다.

## factory 함수

```cpp
std::unique_ptr<ICamera> MakeSyntheticCamera(int width, int height, int fps);
```

호출자가 concrete class 이름을 몰라도 interface 객체를 만들 수 있습니다. 실제 adapter로 교체해도 `main`과 `Pipeline` 변경을 줄여 줍니다.

## `[[nodiscard]]`

반환값을 무시하면 compiler가 경고할 수 있게 합니다.

```cpp
[[nodiscard]] PipelineStats Stats() const;
```

상태나 오류 결과를 실수로 버리는 것을 방지할 때 사용합니다.

## 종료 signal

```cpp
volatile std::sig_atomic_t running = 1;
void HandleSignal(int) { running = 0; }
```

signal handler 안에서는 logging, allocation, mutex 같은 일반 함수를 호출하면 안전하지 않을 수 있습니다. handler는 flag만 바꾸고 정상 thread가 `Pipeline::Stop()`을 호출합니다.
