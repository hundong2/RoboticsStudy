#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <utility>

namespace rv {

template <typename T>
class LatestQueue {
 public:
  explicit LatestQueue(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

  // Real-time policy: discard the oldest work rather than accumulate latency.
  bool Push(T value) {
    std::scoped_lock lock(mutex_);
    if (closed_) return false;
    if (items_.size() == capacity_) {
      items_.pop_front();
      ++dropped_;
    }
    items_.push_back(std::move(value));
    ready_.notify_one();
    return true;
  }

  std::optional<T> Pop(std::stop_token stop) {
    std::unique_lock lock(mutex_);
    ready_.wait(lock, stop, [this] { return closed_ || !items_.empty(); });
    if (items_.empty()) return std::nullopt;
    T value = std::move(items_.front());
    items_.pop_front();
    return value;
  }

  void Close() {
    std::scoped_lock lock(mutex_);
    closed_ = true;
    ready_.notify_all();
  }

  [[nodiscard]] std::uint64_t dropped() const {
    std::scoped_lock lock(mutex_);
    return dropped_;
  }

 private:
  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable_any ready_;
  std::deque<T> items_;
  std::uint64_t dropped_{};
  bool closed_{};
};

}  // namespace rv
