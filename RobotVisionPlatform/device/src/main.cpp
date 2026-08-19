#include "rv/pipeline.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {
// signal handler에서는 async-signal-safe한 sig_atomic_t 값만 바꿉니다.
volatile std::sig_atomic_t running = 1;
void HandleSignal(int) { running = 0; }
}  // namespace

int main(int argc, char** argv) {
  // systemd SIGTERM과 터미널 Ctrl+C(SIGINT)를 같은 정상 종료 경로로 연결합니다.
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  std::string device_id = "jetson-dev-001";
  std::string server_host;
  std::string server_port = "5080";
  // 현재 CLI는 --option value 쌍만 받는 작은 parser입니다.
  for (int index = 1; index + 1 < argc; index += 2) {
    const std::string_view option = argv[index];
    if (option == "--device-id") device_id = argv[index + 1];
    if (option == "--server-host") server_host = argv[index + 1];
    if (option == "--server-port") server_port = argv[index + 1];
  }

  std::unique_ptr<rv::IEventSink> sink;
#ifdef RV_HAS_BOOST_HTTP
  // Boost가 빌드에 포함되고 server host가 있을 때만 HTTP sink를 선택합니다.
  if (!server_host.empty()) {
    sink = rv::MakeHttpEventSink(server_host, server_port, "/api/events/detections");
  }
#endif
  if (!sink) {
    if (!server_host.empty()) {
      std::cerr << "HTTP sink unavailable; rebuild with Boost.System installed\n";
    }
    sink = rv::MakeConsoleEventSink();
  }

  // adapter를 생성해 Pipeline에 소유권을 넘깁니다. 실제 Jetson에서는 이 세 factory를 교체합니다.
  rv::Pipeline pipeline({.device_id = device_id, .frame_queue_capacity = 2},
                        rv::MakeSyntheticCamera(1280, 720, 15), rv::MakeDemoDetector(),
                        std::move(sink));
  pipeline.Start();
  std::cout << "robot-vision device started; Ctrl+C to stop\n";
  // main thread는 signal만 감시하고 실제 작업은 jthread worker가 담당합니다.
  while (running) std::this_thread::sleep_for(std::chrono::milliseconds(200));
  pipeline.Stop();
  const auto stats = pipeline.Stats();
  std::cout << "captured=" << stats.captured << " inferred=" << stats.inferred
            << " published=" << stats.published << " dropped=" << stats.dropped << '\n';
  return 0;
}
