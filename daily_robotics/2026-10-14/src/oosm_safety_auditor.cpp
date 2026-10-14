#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "daily_robotics_2026_10_14/msg/audit_status.hpp"
#include "daily_robotics_2026_10_14/msg/estimator_status.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace
{
constexpr std::size_t kMatchCapacity = 128U;
constexpr std::uint64_t kWarmupSamples = 80U;
constexpr std::uint64_t kRequiredAuditSamples = 250U;
constexpr double kMaxObservedReplayUs = 5000.0;
}  // namespace

class OosmSafetyAuditor final : public rclcpp::Node
{
public:
  OosmSafetyAuditor()
  : Node("oosm_safety_auditor")
  {
    const auto sensor_qos = rclcpp::SensorDataQoS().keep_last(32);
    truth_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      "/study/ground_truth", sensor_qos,
      std::bind(&OosmSafetyAuditor::on_truth, this, std::placeholders::_1));
    status_sub_ = create_subscription<daily_robotics_2026_10_14::msg::EstimatorStatus>(
      "/study/estimator_status", sensor_qos,
      std::bind(&OosmSafetyAuditor::on_status, this, std::placeholders::_1));

    // 감사 결과는 저율 제어/테스트 신호이므로 reliable QoS를 사용한다.
    audit_pub_ = create_publisher<daily_robotics_2026_10_14::msg::AuditStatus>(
      "/study/audit_status", rclcpp::QoS(10).reliable());
    pass_pub_ = create_publisher<std_msgs::msg::Bool>(
      "/study/audit_pass", rclcpp::QoS(10).reliable());

    RCLCPP_INFO(get_logger(), "independent timestamp-matched RMSE audit started");
  }

private:
  struct TruthRecord
  {
    std::int64_t stamp_ns{0};
    double position{0.0};
    bool valid{false};
  };

  struct StatusRecord
  {
    std::int64_t stamp_ns{0};
    daily_robotics_2026_10_14::msg::EstimatorStatus status{};
    bool valid{false};
  };

  // Ground truth와 estimator를 arrival 순서로 비교하지 않고 Header.stamp가 같은 sample끼리만 저장/매칭한다.
  void on_truth(const nav_msgs::msg::Odometry::SharedPtr message)
  {
    TruthRecord & slot = truth_records_[truth_cursor_];
    slot.stamp_ns = rclcpp::Time(message->header.stamp).nanoseconds();
    slot.position = message->pose.pose.position.x;
    slot.valid = true;
    truth_cursor_ = (truth_cursor_ + 1U) % kMatchCapacity;
    try_match(slot.stamp_ns);
  }

  void on_status(
    const daily_robotics_2026_10_14::msg::EstimatorStatus::SharedPtr message)
  {
    StatusRecord & slot = status_records_[status_cursor_];
    slot.stamp_ns = rclcpp::Time(message->header.stamp).nanoseconds();
    slot.status = *message;
    slot.valid = true;
    status_cursor_ = (status_cursor_ + 1U) % kMatchCapacity;
    try_match(slot.stamp_ns);
  }

  // 두 topic의 callback 순서가 바뀌어도 같은 nanosecond stamp가 모두 도착했을 때만 오차를 누적한다.
  void try_match(const std::int64_t stamp_ns)
  {
    TruthRecord * truth = nullptr;
    StatusRecord * status = nullptr;
    for (auto & candidate : truth_records_) {
      if (candidate.valid && candidate.stamp_ns == stamp_ns) {
        truth = &candidate;
        break;
      }
    }
    for (auto & candidate : status_records_) {
      if (candidate.valid && candidate.stamp_ns == stamp_ns) {
        status = &candidate;
        break;
      }
    }
    if (truth == nullptr || status == nullptr) {
      return;
    }

    process_pair(*truth, *status);
    // 같은 sample을 다시 세지 않도록 matched record를 즉시 무효화한다.
    truth->valid = false;
    status->valid = false;
  }

  // 이 함수는 estimator와 독립적으로 RMSE와 bounded-work 계약을 검사한다.
  void process_pair(const TruthRecord & truth, const StatusRecord & status_record)
  {
    const auto & status = status_record.status;
    max_replay_steps_ = std::max(max_replay_steps_, status.last_replay_steps);
    max_replay_time_us_ = std::max(max_replay_time_us_, status.max_replay_time_us);
    latest_accepted_ = status.accepted_delayed_measurements;
    latest_dropped_ = status.dropped_too_old_measurements;

    if (warmup_seen_ < kWarmupSamples) {
      ++warmup_seen_;
      return;
    }

    const double replay_error = status.replay_position - truth.position;
    const double naive_error = status.naive_position - truth.position;
    replay_squared_error_sum_ += replay_error * replay_error;
    naive_squared_error_sum_ += naive_error * naive_error;
    ++matched_samples_;

    const double replay_rmse =
      std::sqrt(replay_squared_error_sum_ / static_cast<double>(matched_samples_));
    const double naive_rmse =
      std::sqrt(naive_squared_error_sum_ / static_cast<double>(matched_samples_));
    const double improvement_percent = naive_rmse > 1.0e-12 ?
      100.0 * (naive_rmse - replay_rmse) / naive_rmse : 0.0;

    // 정확도와 RT 경계를 함께 확인한다. 시간 threshold는 Docker smoke의 회귀 감시값이지 WCET 보증이 아니다.
    const bool pass =
      matched_samples_ >= kRequiredAuditSamples &&
      latest_accepted_ >= 30U &&
      latest_dropped_ == 0U &&
      max_replay_steps_ < kHistoryContractLimit &&
      max_replay_time_us_ < kMaxObservedReplayUs &&
      replay_rmse < 0.12 &&
      replay_rmse < 0.75 * naive_rmse;

    daily_robotics_2026_10_14::msg::AuditStatus audit;
    audit.header = status.header;
    audit.matched_samples = matched_samples_;
    audit.replay_rmse = replay_rmse;
    audit.naive_rmse = naive_rmse;
    audit.improvement_percent = improvement_percent;
    audit.accepted_delayed_measurements = latest_accepted_;
    audit.dropped_too_old_measurements = latest_dropped_;
    audit.max_replay_steps = max_replay_steps_;
    audit.max_replay_time_us = max_replay_time_us_;
    audit.passed = pass;
    audit_pub_->publish(audit);

    std_msgs::msg::Bool pass_message;
    pass_message.data = pass;
    pass_pub_->publish(pass_message);

    if ((matched_samples_ % 100U) == 0U) {
      RCLCPP_INFO(
        get_logger(),
        "samples=%lu replay_rmse=%.5f naive_rmse=%.5f improvement=%.1f%% replay_steps=%u time=%.2fus",
        static_cast<unsigned long>(matched_samples_), replay_rmse, naive_rmse,
        improvement_percent, max_replay_steps_, max_replay_time_us_);
    }
    if (pass && !pass_logged_) {
      pass_logged_ = true;
      RCLCPP_INFO(
        get_logger(),
        "AUDIT_PASS samples=%lu replay_rmse=%.6f naive_rmse=%.6f improvement=%.1f%% "
        "accepted=%lu dropped=%lu max_steps=%u max_replay_us=%.3f",
        static_cast<unsigned long>(matched_samples_), replay_rmse, naive_rmse,
        improvement_percent, static_cast<unsigned long>(latest_accepted_),
        static_cast<unsigned long>(latest_dropped_), max_replay_steps_, max_replay_time_us_);
    }
  }

  // Estimator history capacity가 64이므로 가능한 최대 replay step은 63이다.
  static constexpr std::uint32_t kHistoryContractLimit = 64U;
  std::array<TruthRecord, kMatchCapacity> truth_records_{};
  std::array<StatusRecord, kMatchCapacity> status_records_{};
  std::size_t truth_cursor_{0U};
  std::size_t status_cursor_{0U};

  std::uint64_t warmup_seen_{0U};
  std::uint64_t matched_samples_{0U};
  std::uint64_t latest_accepted_{0U};
  std::uint64_t latest_dropped_{0U};
  std::uint32_t max_replay_steps_{0U};
  double replay_squared_error_sum_{0.0};
  double naive_squared_error_sum_{0.0};
  double max_replay_time_us_{0.0};
  bool pass_logged_{false};

  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr truth_sub_;
  rclcpp::Subscription<daily_robotics_2026_10_14::msg::EstimatorStatus>::SharedPtr status_sub_;
  rclcpp::Publisher<daily_robotics_2026_10_14::msg::AuditStatus>::SharedPtr audit_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pass_pub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<OosmSafetyAuditor>());
  rclcpp::shutdown();
  return 0;
}
