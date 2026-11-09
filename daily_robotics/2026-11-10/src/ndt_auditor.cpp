#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "daily_robotics_2026_11_10/msg/ndt_stats.hpp"
#include "daily_robotics_2026_11_10/msg/ndt_truth.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{

constexpr std::size_t kRingCapacity = 128U;
constexpr std::uint32_t kRequiredSamples = 60U;
constexpr double kPi = 3.14159265358979323846;

double normalize_angle(double angle)
{
  while (angle > kPi) {
    angle -= 2.0 * kPi;
  }
  while (angle < -kPi) {
    angle += 2.0 * kPi;
  }
  return angle;
}

std::int64_t stamp_to_nanoseconds(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1'000'000'000LL +
         static_cast<std::int64_t>(stamp.nanosec);
}

struct TruthSlot
{
  std::int64_t stamp_nanoseconds{};
  daily_robotics_2026_11_10::msg::NdtTruth value{};
  bool valid{};
};

struct StatsSlot
{
  std::int64_t stamp_nanoseconds{};
  daily_robotics_2026_11_10::msg::NdtStats value{};
  bool valid{};
};

}  // namespace

/**
 * matcher와 독립된 노드가 같은 acquisition stamp의 truth/result만 짝지어 정확도, solver 상태,
 * bounded-work telemetry를 검사한다. matcher 내부 self-report만 믿지 않는 안전 구조의 축소판이다.
 */
class NdtAuditor : public rclcpp::Node
{
public:
  NdtAuditor()
  : Node("ndt_auditor")
  {
    truth_subscription_ = create_subscription<daily_robotics_2026_11_10::msg::NdtTruth>(
      "/ndt/truth", rclcpp::QoS(20).reliable(),
      std::bind(&NdtAuditor::on_truth, this, std::placeholders::_1));
    stats_subscription_ = create_subscription<daily_robotics_2026_11_10::msg::NdtStats>(
      "/ndt/stats", rclcpp::QoS(10).reliable(),
      std::bind(&NdtAuditor::on_stats, this, std::placeholders::_1));

    // smoke test가 늦게 붙어도 마지막 판정을 받을 수 있도록 결과는 durable snapshot으로 만든다.
    pass_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/study/pass", rclcpp::QoS(1).reliable().transient_local());
  }

private:
  void on_truth(const daily_robotics_2026_11_10::msg::NdtTruth::SharedPtr message)
  {
    const std::int64_t stamp = stamp_to_nanoseconds(message->header.stamp);
    truths_[truth_cursor_] = TruthSlot{stamp, *message, true};
    truth_cursor_ = (truth_cursor_ + 1U) % kRingCapacity;
    for (StatsSlot & pending : pending_stats_) {
      if (pending.valid && pending.stamp_nanoseconds == stamp) {
        process_pair(*message, pending.value);
        pending.valid = false;
      }
    }
  }

  void on_stats(const daily_robotics_2026_11_10::msg::NdtStats::SharedPtr message)
  {
    const std::int64_t stamp = stamp_to_nanoseconds(message->header.stamp);
    for (const TruthSlot & truth : truths_) {
      if (truth.valid && truth.stamp_nanoseconds == stamp) {
        process_pair(truth.value, *message);
        return;
      }
    }
    // DDS topic 간 순서가 바뀌어 stats가 먼저 와도 fixed ring에서 나중 truth를 기다린다.
    pending_stats_[stats_cursor_] = StatsSlot{stamp, *message, true};
    stats_cursor_ = (stats_cursor_ + 1U) % kRingCapacity;
  }

  void process_pair(
    const daily_robotics_2026_11_10::msg::NdtTruth & truth,
    const daily_robotics_2026_11_10::msg::NdtStats & stats)
  {
    const double dx = stats.estimated_x - truth.true_x;
    const double dy = stats.estimated_y - truth.true_y;
    const double position_error = std::hypot(dx, dy);
    const double yaw_error = std::abs(normalize_angle(stats.estimated_yaw - truth.true_yaw));
    sum_position_error_squared_ += position_error * position_error;
    sum_yaw_error_squared_ += yaw_error * yaw_error;
    maximum_position_error_ = std::max(maximum_position_error_, position_error);
    maximum_yaw_error_ = std::max(maximum_yaw_error_, yaw_error);
    maximum_kernel_time_us_ = std::max(maximum_kernel_time_us_, stats.kernel_time_us);
    minimum_valid_points_ = std::min(minimum_valid_points_, stats.valid_points);
    total_solve_failures_ += stats.solve_failures;
    if (!stats.converged) {
      ++nonconverged_samples_;
    }
    ++matched_samples_;

    if (matched_samples_ < kRequiredSamples) {
      return;
    }
    const double sample_count = static_cast<double>(matched_samples_);
    const double position_rmse = std::sqrt(sum_position_error_squared_ / sample_count);
    const double yaw_rmse = std::sqrt(sum_yaw_error_squared_ / sample_count);
    const bool passed =
      position_rmse < 0.060 && yaw_rmse < 0.030 &&
      maximum_position_error_ < 0.150 && maximum_yaw_error_ < 0.080 &&
      minimum_valid_points_ >= 30U && total_solve_failures_ == 0U &&
      nonconverged_samples_ == 0U;

    std_msgs::msg::Bool result;
    result.data = passed;
    pass_publisher_->publish(result);

    if (matched_samples_ == kRequiredSamples) {
      if (passed) {
        RCLCPP_INFO(
          get_logger(),
          "AUDIT PASS samples=%u position_rmse=%.6f yaw_rmse=%.6f max_position=%.6f "
          "max_yaw=%.6f min_points=%u solve_failures=%u max_kernel_us=%.3f",
          matched_samples_, position_rmse, yaw_rmse, maximum_position_error_,
          maximum_yaw_error_, minimum_valid_points_, total_solve_failures_, maximum_kernel_time_us_);
      } else {
        RCLCPP_ERROR(
          get_logger(),
          "AUDIT FAIL samples=%u position_rmse=%.6f yaw_rmse=%.6f max_position=%.6f "
          "max_yaw=%.6f min_points=%u solve_failures=%u nonconverged=%u",
          matched_samples_, position_rmse, yaw_rmse, maximum_position_error_,
          maximum_yaw_error_, minimum_valid_points_, total_solve_failures_, nonconverged_samples_);
      }
    }
  }

  std::array<TruthSlot, kRingCapacity> truths_{};
  std::array<StatsSlot, kRingCapacity> pending_stats_{};
  std::size_t truth_cursor_{0U};
  std::size_t stats_cursor_{0U};
  std::uint32_t matched_samples_{0U};
  std::uint32_t minimum_valid_points_{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t total_solve_failures_{0U};
  std::uint32_t nonconverged_samples_{0U};
  double sum_position_error_squared_{0.0};
  double sum_yaw_error_squared_{0.0};
  double maximum_position_error_{0.0};
  double maximum_yaw_error_{0.0};
  double maximum_kernel_time_us_{0.0};
  rclcpp::Subscription<daily_robotics_2026_11_10::msg::NdtTruth>::SharedPtr truth_subscription_;
  rclcpp::Subscription<daily_robotics_2026_11_10::msg::NdtStats>::SharedPtr stats_subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<NdtAuditor>());
  rclcpp::shutdown();
  return 0;
}
