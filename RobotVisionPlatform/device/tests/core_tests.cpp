#include "rv/bounded_queue.hpp"
#include "rv/pipeline.hpp"

#include <cassert>
#include <chrono>
#include <thread>

int main() {
  // 용량 2에 세 값을 넣으면 가장 오래된 1이 제거되어야 합니다.
  rv::LatestQueue<int> queue(2);
  assert(queue.Push(1));
  assert(queue.Push(2));
  assert(queue.Push(3));
  assert(queue.dropped() == 1);
  std::stop_source stop;
  assert(queue.Pop(stop.get_token()).value() == 2);

  // 외부 카메라나 서버 없이 adapter 조합과 thread 종료를 smoke test합니다.
  rv::Pipeline pipeline({.device_id = "test", .frame_queue_capacity = 2},
                        rv::MakeSyntheticCamera(32, 32, 100), rv::MakeDemoDetector(),
                        rv::MakeConsoleEventSink());
  pipeline.Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  pipeline.Stop();
  const auto stats = pipeline.Stats();
  assert(stats.captured > 0);
  assert(stats.inferred > 0);
  return 0;
}
