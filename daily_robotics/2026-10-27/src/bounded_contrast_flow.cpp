#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#include "daily_robotics_2026_10_27/msg/event_motion_stats.hpp"
#include "daily_robotics_2026_10_27/msg/event_packet.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr std::size_t kMaxEvents = 128U;
constexpr std::size_t kQueueSlots = 4U;
constexpr std::size_t kWidth = 64U;
constexpr std::size_t kHeight = 48U;
constexpr std::size_t kPixels = kWidth * kHeight;
constexpr std::size_t kPolarityBins = 2U * kPixels;
constexpr std::size_t kVelocitySteps = 7U;
constexpr std::size_t kHypothesisCount = kVelocitySteps * kVelocitySteps;
constexpr double kVelocityMinimumPxS = -300.0;
constexpr double kVelocityStepPxS = 100.0;

// 한 DDS producer와 한 numerical worker만 사용하는 fixed-capacity SPSC ring이다.
// release/acquire는 payload pointer 기록이 끝난 뒤에 consumer가 그 값을 보도록 순서를 만든다.
template<typename T, std::size_t Capacity>
class SpscRing
{
  static_assert(Capacity >= 2U, "ring needs a sentinel slot");

public:
  bool push(T value)
  {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = (head + 1U) % Capacity;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    storage_[head] = std::move(value);
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T & value)
  {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    value = std::move(storage_[tail]);
    storage_[tail].reset();
    tail_.store((tail + 1U) % Capacity, std::memory_order_release);
    return true;
  }

private:
  std::array<T, Capacity> storage_{};
  std::atomic<std::size_t> head_{0U};
  std::atomic<std::size_t> tail_{0U};
};
}  // namespace

class BoundedContrastFlow final : public rclcpp::Node
{
public:
  BoundedContrastFlow()
  : Node("bounded_contrast_flow")
  {
    // subscription과 numerical timer를 서로 다른 MutuallyExclusive callback group에 둔다.
    // MultiThreadedExecutor의 두 worker가 DDS 수신과 계산을 격리하되, 각 group 내부 재진입은 막는다.
    input_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    worker_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions input_options;
    input_options.callback_group = input_group_;
    event_subscription_ = create_subscription<daily_robotics_2026_10_27::msg::EventPacket>(
      "/event_camera/events", rclcpp::SensorDataQoS(),
      std::bind(&BoundedContrastFlow::on_packet, this, std::placeholders::_1), input_options);

    // 결과는 auditor가 누락 없이 수집해야 하므로 Reliable QoS를 사용한다.
    stats_publisher_ = create_publisher<daily_robotics_2026_10_27::msg::EventMotionStats>(
      "/event_camera/motion_stats", rclcpp::QoS(10).reliable());

    // 200 Hz worker는 wake-up당 packet 하나만 처리한다. producer가 과부하되면 callback 시간을
    // 늘리지 않고 fixed ring drop counter를 올려 overload를 외부 telemetry로 드러낸다.
    worker_timer_ = create_wall_timer(
      5ms, std::bind(&BoundedContrastFlow::process_one_packet, this), worker_group_);

    RCLCPP_INFO(
      get_logger(),
      "bounded contrast flow: <=%zu events, exactly %zu hypotheses, one packet/wakeup",
      kMaxEvents, kHypothesisCount);
  }

private:
  using PacketConstPtr = daily_robotics_2026_10_27::msg::EventPacket::ConstSharedPtr;

  // DDS callback의 역할은 수치 계산이 아니라 immutable message pointer의 ownership handoff다.
  void on_packet(PacketConstPtr packet)
  {
    if (!packet_queue_.push(std::move(packet))) {
      queue_drops_.fetch_add(1U, std::memory_order_relaxed);
    }
  }

  // Header/frame, fixed image geometry, 좌표, 상대시각 계약을 검사한다.
  bool validate_packet(
    const daily_robotics_2026_10_27::msg::EventPacket & packet,
    std::size_t & valid_events) const
  {
    const auto count = std::min<std::size_t>(packet.count, kMaxEvents);
    bool valid = packet.header.frame_id == "event_camera_optical_frame" &&
      packet.width == kWidth && packet.height == kHeight &&
      packet.count > 0U && packet.count <= kMaxEvents;
    valid_events = 0U;

    for (std::size_t i = 0U; i < count; ++i) {
      // dt_us는 header.stamp 기준의 event 측정시각이며 미래 event(양수)나 너무 오래된 event를 거부한다.
      const bool event_valid = packet.x[i] < kWidth && packet.y[i] < kHeight &&
        packet.dt_us[i] <= 0 && packet.dt_us[i] >= -40000;
      valid = valid && event_valid;
      if (event_valid) {
        ++valid_events;
      }
    }
    return valid;
  }

  // 후보 속도로 모든 event를 packet 끝 시각에 motion-compensate하고 contrast를 계산한다.
  double contrast_for_velocity(
    const daily_robotics_2026_10_27::msg::EventPacket & packet,
    const std::size_t valid_events, const double vx_px_s, const double vy_px_s)
  {
    // fill은 2*64*48 fixed histogram만 초기화한다. heap allocation이나 크기 가변 container가 없다.
    histogram_.fill(0U);
    std::size_t warped_count = 0U;

    for (std::size_t i = 0U; i < valid_events; ++i) {
      const double seconds_to_reference = -static_cast<double>(packet.dt_us[i]) * 1.0e-6;
      // 논문의 등속 warp: x'_k=x_k+v_x(t_ref-t_k), y'_k=y_k+v_y(t_ref-t_k).
      // 올바른 v이면 시간축을 따라 퍼진 같은 edge event가 기준 시각의 같은 pixel에 모인다.
      const long warped_x = std::lround(
        static_cast<double>(packet.x[i]) + vx_px_s * seconds_to_reference);
      const long warped_y = std::lround(
        static_cast<double>(packet.y[i]) + vy_px_s * seconds_to_reference);
      if (warped_x < 0L || warped_x >= static_cast<long>(kWidth) ||
        warped_y < 0L || warped_y >= static_cast<long>(kHeight))
      {
        continue;
      }

      const auto polarity_offset = packet.polarity[i] ? kPixels : 0U;
      const auto pixel = static_cast<std::size_t>(warped_y) * kWidth +
        static_cast<std::size_t>(warped_x);
      ++histogram_[polarity_offset + pixel];
      ++warped_count;
    }

    if (warped_count == 0U) {
      return 0.0;
    }

    // Var(H)=mean(H^2)-mean(H)^2. event 수가 같으면 올바르게 정렬되어 peak가 높을수록 증가한다.
    double square_sum = 0.0;
    for (const auto count : histogram_) {
      const double value = static_cast<double>(count);
      square_sum += value * value;
    }
    const double mean = static_cast<double>(warped_count) /
      static_cast<double>(kPolarityBins);
    return square_sum / static_cast<double>(kPolarityBins) - mean * mean;
  }

  // 한 packet에 대해 7x7 velocity grid를 정확히 한 번 평가해 최댓값을 찾는다.
  void process_one_packet()
  {
    PacketConstPtr packet;
    if (!packet_queue_.pop(packet) || !packet) {
      return;
    }

    const auto start = std::chrono::steady_clock::now();
    std::size_t valid_events = 0U;
    const bool contract_valid = validate_packet(*packet, valid_events);
    double best_contrast = -std::numeric_limits<double>::infinity();
    double second_contrast = -std::numeric_limits<double>::infinity();
    double best_vx = 0.0;
    double best_vy = 0.0;
    std::size_t hypotheses_evaluated = 0U;

    if (contract_valid) {
      for (std::size_t y_index = 0U; y_index < kVelocitySteps; ++y_index) {
        const double candidate_vy = kVelocityMinimumPxS +
          kVelocityStepPxS * static_cast<double>(y_index);
        for (std::size_t x_index = 0U; x_index < kVelocitySteps; ++x_index) {
          const double candidate_vx = kVelocityMinimumPxS +
            kVelocityStepPxS * static_cast<double>(x_index);
          const double contrast = contrast_for_velocity(
            *packet, valid_events, candidate_vx, candidate_vy);
          ++hypotheses_evaluated;
          if (contrast > best_contrast) {
            second_contrast = best_contrast;
            best_contrast = contrast;
            best_vx = candidate_vx;
            best_vy = candidate_vy;
          } else if (contrast > second_contrast) {
            second_contrast = contrast;
          }
        }
      }
    }

    const auto stop = std::chrono::steady_clock::now();
    const double kernel_us = std::chrono::duration<double, std::micro>(stop - start).count();
    max_kernel_us_ = std::max(max_kernel_us_, kernel_us);
    const double peak_ratio = second_contrast > 0.0 ? best_contrast / second_contrast : 0.0;
    const bool peak_is_observable = contract_valid && valid_events >= 64U && peak_ratio > 1.001;

    daily_robotics_2026_10_27::msg::EventMotionStats stats;
    stats.header = packet->header;
    stats.sequence = packet->sequence;
    stats.input_events = packet->count;
    stats.valid_events = static_cast<std::uint32_t>(valid_events);
    stats.hypotheses_evaluated = static_cast<std::uint32_t>(hypotheses_evaluated);
    stats.queue_drops = queue_drops_.load(std::memory_order_relaxed);
    stats.estimated_vx_px_s = best_vx;
    stats.estimated_vy_px_s = best_vy;
    stats.best_contrast = best_contrast;
    stats.second_contrast = second_contrast;
    stats.peak_ratio = peak_ratio;
    stats.latest_kernel_us = kernel_us;
    stats.max_kernel_us = max_kernel_us_;
    stats.packet_contract_valid = contract_valid;
    stats.peak_is_observable = peak_is_observable;
    stats.bounds_respected = valid_events <= kMaxEvents &&
      hypotheses_evaluated <= kHypothesisCount;
    stats_publisher_->publish(stats);
  }

  rclcpp::CallbackGroup::SharedPtr input_group_;
  rclcpp::CallbackGroup::SharedPtr worker_group_;
  rclcpp::Subscription<daily_robotics_2026_10_27::msg::EventPacket>::SharedPtr
    event_subscription_;
  rclcpp::Publisher<daily_robotics_2026_10_27::msg::EventMotionStats>::SharedPtr
    stats_publisher_;
  rclcpp::TimerBase::SharedPtr worker_timer_;

  SpscRing<PacketConstPtr, kQueueSlots> packet_queue_;
  std::array<std::uint16_t, kPolarityBins> histogram_{};
  std::atomic<std::uint32_t> queue_drops_{0U};
  double max_kernel_us_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<BoundedContrastFlow>();
  // 두 executor worker가 input/worker callback group을 병렬로 서비스한다.
  // 이는 callback 격리일 뿐, OS thread priority와 DDS까지 포함한 hard-RT 보장은 아니다.
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2U);
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
