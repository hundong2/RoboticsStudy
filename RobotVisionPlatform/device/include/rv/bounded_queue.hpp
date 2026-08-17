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

/// 실시간 처리용 크기 제한 producer-consumer queue입니다.
///
/// queue가 가득 차면 producer를 막지 않고 가장 오래된 항목을 제거합니다.
/// 이 정책은 모든 프레임 보존보다 최신 화면의 낮은 지연이 중요한 경우에 적합합니다.
template <typename T>
class LatestQueue {
 public:
  /// @param capacity 보관할 최대 항목 수입니다. 0은 안전하게 1로 보정됩니다.
  explicit LatestQueue(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

  /// 값을 queue 뒤에 추가합니다. 가득 찼으면 가장 오래된 값을 먼저 버립니다.
  /// @return queue가 열려 있어 추가됐으면 true, Close() 이후면 false입니다.
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

  /// 값이 생기거나 queue가 닫히거나 stop 요청이 올 때까지 기다립니다.
  /// @return 값이 있으면 이동 반환하고, 정상 종료 조건이면 nullopt를 반환합니다.
  std::optional<T> Pop(std::stop_token stop) {
    std::unique_lock lock(mutex_);
    // condition_variable_any의 C++20 overload는 stop 요청도 wake-up 조건으로 처리합니다.
    ready_.wait(lock, stop, [this] { return closed_ || !items_.empty(); });
    if (items_.empty()) return std::nullopt;
    T value = std::move(items_.front());
    items_.pop_front();
    return value;
  }

  /// 이후 Push를 거부하고 대기 중인 모든 consumer를 깨웁니다.
  void Close() {
    std::scoped_lock lock(mutex_);
    closed_ = true;
    ready_.notify_all();
  }

  /// queue 포화로 제거된 항목의 누적 수를 thread-safe하게 반환합니다.
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
