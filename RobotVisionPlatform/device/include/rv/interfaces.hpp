#pragma once

#include "rv/types.hpp"

#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace rv {

class ICamera {
 public:
  virtual ~ICamera() = default;
  virtual Frame Read(std::stop_token stop) = 0;
};

class IDetector {
 public:
  virtual ~IDetector() = default;
  virtual std::vector<Detection> Infer(const Frame& frame) = 0;
  [[nodiscard]] virtual std::string Version() const = 0;
};

class IEventSink {
 public:
  virtual ~IEventSink() = default;
  virtual bool Publish(const DetectionEvent& event) = 0;
};

std::unique_ptr<ICamera> MakeSyntheticCamera(int width, int height, int fps);
std::unique_ptr<IDetector> MakeDemoDetector();
std::unique_ptr<IEventSink> MakeConsoleEventSink();

#ifdef RV_HAS_BOOST_HTTP
std::unique_ptr<IEventSink> MakeHttpEventSink(std::string host, std::string port,
                                              std::string target);
#endif

}  // namespace rv

