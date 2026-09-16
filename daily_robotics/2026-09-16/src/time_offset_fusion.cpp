#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>

#include "daily_robotics_2026_09_16/msg/calibration_status.hpp"
#include "geometry_msgs/msg/twist_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

namespace daily_robotics
{

struct Sample
{
  double stamp_seconds{0.0};
  double yaw_rate{0.0};
  double variance{1.0};
};

// FixedRing은 callback hot path에서 vector growth/heap allocation이 일어나지 않게 하는 고정용량 ring이다.
// 가득 차면 가장 오래된 표본을 덮어쓰므로 메모리 상한과 데이터 age 상한이 명확하다.
template<std::size_t Capacity>
class FixedRing
{
public:
  void push(const Sample & sample) noexcept
  {
    data_[next_] = sample;
    next_ = (next_ + 1U) % Capacity;
    size_ = std::min(size_ + 1U, Capacity);
  }

  // 내부 ring 순서를 timestamp 오름차순 배열로 풀어낸다. 반환값은 유효한 원소 개수다.
  std::size_t copy_chronological(std::array<Sample, Capacity> & output) const noexcept
  {
    const std::size_t oldest = (next_ + Capacity - size_) % Capacity;
    for (std::size_t i = 0; i < size_; ++i) {
      output[i] = data_[(oldest + i) % Capacity];
    }
    return size_;
  }

private:
  std::array<Sample, Capacity> data_{};
  std::size_t next_{0U};
  std::size_t size_{0U};
};

struct OffsetScore
{
  bool valid{false};
  double weighted_cost{std::numeric_limits<double>::infinity()};
  double rmse{std::numeric_limits<double>::infinity()};
  std::uint32_t pairs{0U};
};

class TimeOffsetFusion final : public rclcpp::Node
{
public:
  // 이 노드는 두 yaw-rate stream을 고정용량 buffer에 모으고, LiDAR stamp에서 후보 Δt를 뺀
  // 시각의 IMU 값을 보간하여 residual이 최소가 되는 clock offset을 추정한다.
  TimeOffsetFusion()
  : Node("time_offset_fusion")
  {
    // MutuallyExclusive callback group은 sensor callback과 solver timer가 동시에 ring을 만지지 못하게 한다.
    // 별도 mutex가 없어도 되는 이유는 아래 main이 SingleThreadedExecutor를 사용하기 때문이다.
    callback_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    rclcpp::SubscriptionOptions options;
    options.callback_group = callback_group_;

    // SensorDataQoS는 발행자와 같은 best-effort/volatile profile을 요청한다. 최신 센서 표본이
    // 오래된 재전송보다 중요하다는 선택이며, drop이 허용되지 않는 계측이라면 바꿔야 한다.
    imu_subscription_ = create_subscription<geometry_msgs::msg::TwistWithCovarianceStamped>(
      "/imu/yaw_rate", rclcpp::SensorDataQoS().keep_last(5),
      std::bind(&TimeOffsetFusion::on_imu, this, std::placeholders::_1), options);
    lidar_subscription_ = create_subscription<geometry_msgs::msg::TwistWithCovarianceStamped>(
      "/lidar/yaw_rate", rclcpp::SensorDataQoS().keep_last(5),
      std::bind(&TimeOffsetFusion::on_lidar, this, std::placeholders::_1), options);

    // 보정 status는 late-joining auditor도 마지막 결과를 즉시 받도록 transient-local depth 1을 쓴다.
    status_publisher_ = create_publisher<daily_robotics_2026_09_16::msg::CalibrationStatus>(
      "/calibration/status", rclcpp::QoS(1).reliable().transient_local());
    fused_publisher_ = create_publisher<geometry_msgs::msg::TwistWithCovarianceStamped>(
      "/fusion/yaw_rate", rclcpp::SensorDataQoS().keep_last(5));

    // 500 ms마다 보정한다. 센서 callback은 O(1) 복사만 수행하고, 무거운 탐색은 timer 경계로 격리한다.
    solver_timer_ = create_wall_timer(
      std::chrono::milliseconds(500), std::bind(&TimeOffsetFusion::solve_and_publish, this),
      callback_group_);
  }

private:
  static constexpr std::size_t kImuCapacity = 1024U;    // 200 Hz에서 약 5.1 s
  static constexpr std::size_t kLidarCapacity = 128U;   // 20 Hz에서 약 6.4 s
  static constexpr double kMinOffsetSeconds = -0.080;
  static constexpr double kMaxOffsetSeconds = 0.080;
  static constexpr double kOffsetStepSeconds = 0.001;
  static constexpr std::size_t kCandidateCount = 161U;
  static constexpr std::uint32_t kMinimumPairs = 30U;

  // builtin_interfaces/Time의 sec/nanosec를 double seconds로 바꾼다.
  // rclcpp::Time을 쓰면 ROS time 표현과 nanosecond 정규화를 직접 처리하지 않아도 된다.
  static double stamp_seconds(const builtin_interfaces::msg::Time & stamp)
  {
    return rclcpp::Time(stamp).seconds();
  }

  // IMU callback의 역할은 메시지에서 보정에 필요한 세 값만 꺼내 고정 ring에 복사하는 것이다.
  void on_imu(const geometry_msgs::msg::TwistWithCovarianceStamped::SharedPtr message)
  {
    imu_ring_.push(Sample{
      stamp_seconds(message->header.stamp), message->twist.twist.angular.z,
      std::max(message->twist.covariance[35], 1.0e-9)});
  }

  // LiDAR callback도 동일하게 O(1)이며, solver가 오래 걸려 sensor callback이 지연되는지 분리해 볼 수 있다.
  void on_lidar(const geometry_msgs::msg::TwistWithCovarianceStamped::SharedPtr message)
  {
    lidar_ring_.push(Sample{
      stamp_seconds(message->header.stamp), message->twist.twist.angular.z,
      std::max(message->twist.covariance[35], 1.0e-9)});
  }

  // target time을 감싸는 두 IMU 표본을 binary search로 찾아 선형 보간한다.
  // 수식은 y(t)=(1-α)y0+αy1, α=(t-t0)/(t1-t0)이며 분산도 같은 비율로 보간한다.
  template<std::size_t Capacity>
  static bool interpolate(
    const std::array<Sample, Capacity> & samples, const std::size_t count,
    const double target_time, Sample & interpolated)
  {
    if (count < 2U || target_time < samples[0].stamp_seconds ||
      target_time > samples[count - 1U].stamp_seconds)
    {
      return false;
    }

    std::size_t low = 0U;
    std::size_t high = count - 1U;
    while (high - low > 1U) {
      const std::size_t middle = low + (high - low) / 2U;
      if (samples[middle].stamp_seconds <= target_time) {
        low = middle;
      } else {
        high = middle;
      }
    }

    const double interval = samples[high].stamp_seconds - samples[low].stamp_seconds;
    if (interval <= 1.0e-9) {
      return false;
    }
    const double alpha = (target_time - samples[low].stamp_seconds) / interval;
    interpolated.stamp_seconds = target_time;
    interpolated.yaw_rate =
      (1.0 - alpha) * samples[low].yaw_rate + alpha * samples[high].yaw_rate;
    interpolated.variance =
      (1.0 - alpha) * samples[low].variance + alpha * samples[high].variance;
    return true;
  }

  // 후보 offset Δt의 비용 J(Δt)=Σ r²/(σ_imu²+σ_lidar²) / Σ 1/(σ_imu²+σ_lidar²)을 계산한다.
  // LiDAR stamp가 실제보다 Δt만큼 앞서 있으므로 IMU 조회 시각은 t_lidar-Δt다.
  OffsetScore score_offset(
    const double offset_seconds, const std::size_t imu_count,
    const std::size_t lidar_count) const
  {
    double weighted_square_sum = 0.0;
    double weight_sum = 0.0;
    double square_sum = 0.0;
    std::uint32_t pairs = 0U;

    for (std::size_t i = 0U; i < lidar_count; ++i) {
      Sample imu_at_lidar_time;
      const double corrected_time = lidar_snapshot_[i].stamp_seconds - offset_seconds;
      if (!interpolate(imu_snapshot_, imu_count, corrected_time, imu_at_lidar_time)) {
        continue;
      }
      const double residual = lidar_snapshot_[i].yaw_rate - imu_at_lidar_time.yaw_rate;
      const double combined_variance = lidar_snapshot_[i].variance + imu_at_lidar_time.variance;
      const double weight = 1.0 / std::max(combined_variance, 1.0e-9);
      weighted_square_sum += weight * residual * residual;
      weight_sum += weight;
      square_sum += residual * residual;
      ++pairs;
    }

    OffsetScore result;
    result.pairs = pairs;
    result.valid = pairs >= kMinimumPairs && weight_sum > 0.0;
    if (result.valid) {
      result.weighted_cost = weighted_square_sum / weight_sum;
      result.rmse = std::sqrt(square_sum / static_cast<double>(pairs));
    }
    return result;
  }

  // 실시간 로봇에서 이 함수는 calibration worker의 역할을 한다. 후보 수, buffer 크기,
  // binary-search 횟수가 모두 상수로 제한되어 계산량 상한을 사전에 산정할 수 있다.
  void solve_and_publish()
  {
    const std::size_t imu_count = imu_ring_.copy_chronological(imu_snapshot_);
    const std::size_t lidar_count = lidar_ring_.copy_chronological(lidar_snapshot_);
    if (imu_count < 100U || lidar_count < kMinimumPairs) {
      return;
    }

    const auto solve_start = std::chrono::steady_clock::now();
    std::size_t best_index = 0U;
    OffsetScore best_score;
    for (std::size_t index = 0U; index < kCandidateCount; ++index) {
      const double candidate = kMinOffsetSeconds + static_cast<double>(index) * kOffsetStepSeconds;
      candidate_scores_[index] = score_offset(candidate, imu_count, lidar_count);
      if (candidate_scores_[index].valid &&
        candidate_scores_[index].weighted_cost < best_score.weighted_cost)
      {
        best_score = candidate_scores_[index];
        best_index = index;
      }
    }
    if (!best_score.valid) {
      return;
    }

    double estimated_offset =
      kMinOffsetSeconds + static_cast<double>(best_index) * kOffsetStepSeconds;
    // 격자 최솟값과 좌우 비용에 포물선을 맞춰 1 ms보다 세밀한 sub-grid offset을 얻는다.
    // Δx = 0.5h(J_- - J_+) / (J_- - 2J_0 + J_+) 이다.
    if (best_index > 0U && best_index + 1U < kCandidateCount &&
      candidate_scores_[best_index - 1U].valid && candidate_scores_[best_index + 1U].valid)
    {
      const double left = candidate_scores_[best_index - 1U].weighted_cost;
      const double center = candidate_scores_[best_index].weighted_cost;
      const double right = candidate_scores_[best_index + 1U].weighted_cost;
      const double curvature = left - 2.0 * center + right;
      if (std::abs(curvature) > 1.0e-12) {
        const double correction =
          std::clamp(0.5 * kOffsetStepSeconds * (left - right) / curvature,
          -kOffsetStepSeconds, kOffsetStepSeconds);
        estimated_offset += correction;
        best_score = score_offset(estimated_offset, imu_count, lidar_count);
      }
    }

    const Sample & lidar = lidar_snapshot_[lidar_count - 1U];
    Sample imu;
    if (!interpolate(imu_snapshot_, imu_count, lidar.stamp_seconds - estimated_offset, imu)) {
      return;
    }

    // 독립 Gaussian noise를 가정한 최소분산 scalar fusion:
    // ω_f=(ω_i/σ_i² + ω_l/σ_l²)/(1/σ_i² + 1/σ_l²), Var(ω_f)=1/(Σ 1/σ²).
    const double imu_weight = 1.0 / imu.variance;
    const double lidar_weight = 1.0 / lidar.variance;
    const double fused_rate =
      (imu_weight * imu.yaw_rate + lidar_weight * lidar.yaw_rate) /
      (imu_weight + lidar_weight);
    const double fused_variance = 1.0 / (imu_weight + lidar_weight);

    geometry_msgs::msg::TwistWithCovarianceStamped fused_message;
    const auto corrected_nanoseconds = static_cast<std::int64_t>(
      (lidar.stamp_seconds - estimated_offset) * 1.0e9);
    const rclcpp::Time corrected_stamp(
      corrected_nanoseconds, get_clock()->get_clock_type());
    fused_message.header.stamp = static_cast<builtin_interfaces::msg::Time>(corrected_stamp);
    fused_message.header.frame_id = "base_link";
    fused_message.twist.twist.angular.z = fused_rate;
    fused_message.twist.covariance.fill(0.0);
    fused_message.twist.covariance[35] = fused_variance;
    fused_publisher_->publish(fused_message);

    const auto solve_end = std::chrono::steady_clock::now();
    const auto solve_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
      solve_end - solve_start).count();

    daily_robotics_2026_09_16::msg::CalibrationStatus status;
    status.header.stamp = static_cast<builtin_interfaces::msg::Time>(now());
    status.header.frame_id = "base_link";
    status.estimated_offset_ms = estimated_offset * 1000.0;
    status.residual_rmse = best_score.rmse;
    status.fused_yaw_rate = fused_rate;
    status.matched_pairs = best_score.pairs;
    status.imu_samples = static_cast<std::uint32_t>(imu_count);
    status.lidar_samples = static_cast<std::uint32_t>(lidar_count);
    status.search_candidates = static_cast<std::uint32_t>(kCandidateCount);
    status.solve_time_us = static_cast<std::uint64_t>(std::max<std::int64_t>(0, solve_microseconds));
    status_publisher_->publish(status);

    // THROTTLE은 console I/O가 solver 주기에 계속 개입하지 않도록 2초에 한 번만 출력한다.
    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 2000,
      "offset=%.3f ms rmse=%.5f pairs=%u solve=%lu us",
      status.estimated_offset_ms, status.residual_rmse, status.matched_pairs,
      static_cast<unsigned long>(status.solve_time_us));
  }

  FixedRing<kImuCapacity> imu_ring_;
  FixedRing<kLidarCapacity> lidar_ring_;
  std::array<Sample, kImuCapacity> imu_snapshot_{};
  std::array<Sample, kLidarCapacity> lidar_snapshot_{};
  std::array<OffsetScore, kCandidateCount> candidate_scores_{};

  rclcpp::CallbackGroup::SharedPtr callback_group_;
  rclcpp::Subscription<geometry_msgs::msg::TwistWithCovarianceStamped>::SharedPtr imu_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::TwistWithCovarianceStamped>::SharedPtr lidar_subscription_;
  rclcpp::Publisher<daily_robotics_2026_09_16::msg::CalibrationStatus>::SharedPtr status_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::TwistWithCovarianceStamped>::SharedPtr fused_publisher_;
  rclcpp::TimerBase::SharedPtr solver_timer_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<daily_robotics::TimeOffsetFusion>();
  // SingleThreadedExecutor는 callback ordering을 직렬화해 ring의 동시 접근을 막는다.
  // 이는 bounded computation을 보여 주지만 OS scheduling/DDS allocation까지 hard RT로 만들지는 않는다.
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
