#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "daily_robotics_2026_10_21/msg/contact_sample.hpp"
#include "daily_robotics_2026_10_21/msg/estimator_stats.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr double kLegLengthM = 0.55;
constexpr double kContactOnN = 80.0;
constexpr double kContactOffN = 40.0;
constexpr std::size_t kQueueCapacity = 32U;
constexpr std::size_t kMaxSamplesPerCycle = 4U;
constexpr double kNisGate2D = 9.21;  // chi-square 2 DoF의 약 99% 경계

struct RuntimeSample
{
  // ROS message의 string/vector를 RT 큐에 복사하지 않고 필요한 고정 크기 값만 추출한다.
  builtin_interfaces::msg::Time stamp{};
  std::int64_t stamp_ns{0};
  double joint_position_rad{0.0};
  double joint_velocity_rad_s{0.0};
  double acceleration_mps2{0.0};
  double normal_force_n{0.0};
};

std::int64_t stamp_to_ns(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1000000000LL +
         static_cast<std::int64_t>(stamp.nanosec);
}

class SampleSpscQueue
{
public:
  bool push(const RuntimeSample & sample)
  {
    // producer만 head를 쓰고 consumer만 tail을 쓴다. release/acquire가 payload 공개 순서를 보장한다.
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1U) % kQueueCapacity;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    storage_[head] = sample;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(RuntimeSample & sample)
  {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    sample = storage_[tail];
    tail_.store((tail + 1U) % kQueueCapacity, std::memory_order_release);
    return true;
  }

private:
  std::array<RuntimeSample, kQueueCapacity> storage_{};
  alignas(64) std::atomic<std::size_t> head_{0U};
  alignas(64) std::atomic<std::size_t> tail_{0U};
};
}  // namespace

class BoundedContactEkf : public rclcpp::Node
{
public:
  BoundedContactEkf()
  : Node("bounded_contact_ekf")
  {
    // 서로 다른 callback group을 두 worker가 실행한다. 입력 callback은 producer, timer는 consumer다.
    input_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    estimator_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions input_options;
    input_options.callback_group = input_group_;
    sample_sub_ = create_subscription<daily_robotics_2026_10_21::msg::ContactSample>(
      "/contact/sample", rclcpp::SensorDataQoS(),
      std::bind(&BoundedContactEkf::on_sample, this, std::placeholders::_1), input_options);

    const auto output_qos = rclcpp::QoS(rclcpp::KeepLast(20)).reliable();
    pose_pub_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/estimator/pose", output_qos);
    stats_pub_ = create_publisher<daily_robotics_2026_10_21::msg::EstimatorStats>(
      "/estimator/stats", output_qos);

    // 세 번째 인자로 callback group을 지정해 200 Hz estimator가 입력 callback과 분리되게 한다.
    estimator_timer_ = create_wall_timer(
      5ms, std::bind(&BoundedContactEkf::on_estimator_cycle, this), estimator_group_);

    // 상태 x=[z, vz, b_a]. P는 같은 순서의 3x3 오차 공분산이다.
    state_ = {0.45, 0.0, 0.0};
    covariance_ = {
      0.04 * 0.04, 0.0, 0.0,
      0.0, 0.20 * 0.20, 0.0,
      0.0, 0.0, 0.08 * 0.08};
  }

private:
  enum class ContactState : std::uint8_t
  {
    kAir = 0U,
    kStance = 1U
  };

  void on_sample(const daily_robotics_2026_10_21::msg::ContactSample::SharedPtr message)
  {
    // ROS 경계에서 frame, timestamp, finite 값을 검증한 뒤 RT-friendly scalar 구조체로 축소한다.
    const std::int64_t stamp_ns = stamp_to_ns(message->header.stamp);
    const bool finite =
      std::isfinite(message->joint_position_rad) &&
      std::isfinite(message->joint_velocity_rad_s) &&
      std::isfinite(message->gravity_compensated_accel_z_mps2) &&
      std::isfinite(message->normal_force_n);
    if (message->header.frame_id != "base_link" || stamp_ns <= 0LL || !finite) {
      invalid_samples_.fetch_add(1U, std::memory_order_relaxed);
      return;
    }

    RuntimeSample sample;
    sample.stamp = message->header.stamp;
    sample.stamp_ns = stamp_ns;
    sample.joint_position_rad = message->joint_position_rad;
    sample.joint_velocity_rad_s = message->joint_velocity_rad_s;
    sample.acceleration_mps2 = message->gravity_compensated_accel_z_mps2;
    sample.normal_force_n = message->normal_force_n;
    if (!sample_queue_.push(sample)) {
      // 큐가 찼을 때 기다리거나 할당하지 않고 drop을 계수해 deadline 영향이 숨지 않게 한다.
      queue_drops_.fetch_add(1U, std::memory_order_relaxed);
    }
  }

  void update_contact_state(const double normal_force_n)
  {
    // ON/OFF threshold를 다르게 둔 Schmitt trigger와 3표본 dwell이 threshold chatter를 막는다.
    if (contact_state_ == ContactState::kAir) {
      low_force_count_ = 0U;
      high_force_count_ = normal_force_n >= kContactOnN ? high_force_count_ + 1U : 0U;
      if (high_force_count_ >= 3U) {
        contact_state_ = ContactState::kStance;
        high_force_count_ = 0U;
        ++contact_transitions_;
      }
    } else {
      high_force_count_ = 0U;
      low_force_count_ = normal_force_n <= kContactOffN ? low_force_count_ + 1U : 0U;
      if (low_force_count_ >= 3U) {
        contact_state_ = ContactState::kAir;
        low_force_count_ = 0U;
        ++contact_transitions_;
      }
    }
  }

  void predict(const double acceleration_mps2, const double dt_s)
  {
    // IMU 모델: z'=z+v*dt+0.5*(a_m-b_a)*dt^2, v'=v+(a_m-b_a)*dt.
    const double unbiased_acceleration = acceleration_mps2 - state_[2];
    state_[0] += state_[1] * dt_s + 0.5 * unbiased_acceleration * dt_s * dt_s;
    state_[1] += unbiased_acceleration * dt_s;

    // 위 비선형식은 이 1D 모델에서 정확히 선형이며 Jacobian F가 아래 3x3 행렬이다.
    const std::array<double, 9> f = {
      1.0, dt_s, -0.5 * dt_s * dt_s,
      0.0, 1.0, -dt_s,
      0.0, 0.0, 1.0};
    std::array<double, 9> fp{};
    std::array<double, 9> predicted{};
    for (std::size_t row = 0U; row < 3U; ++row) {
      for (std::size_t column = 0U; column < 3U; ++column) {
        for (std::size_t inner = 0U; inner < 3U; ++inner) {
          fp[row * 3U + column] +=
            f[row * 3U + inner] * covariance_[inner * 3U + column];
        }
      }
    }
    for (std::size_t row = 0U; row < 3U; ++row) {
      for (std::size_t column = 0U; column < 3U; ++column) {
        for (std::size_t inner = 0U; inner < 3U; ++inner) {
          // P'=F P F^T이므로 두 번째 F의 index는 [column, inner]가 된다.
          predicted[row * 3U + column] +=
            fp[row * 3U + inner] * f[column * 3U + inner];
        }
      }
    }
    // Q는 한 step의 모델 오차다. 고정 수치이며 데이터에 따라 배열 크기나 반복 수가 늘지 않는다.
    predicted[0] += 1.0e-8;
    predicted[4] += 2.0e-5;
    predicted[8] += 1.0e-7;
    covariance_ = predicted;
  }

  void scalar_update(const std::size_t observed_state, const double residual, const double variance)
  {
    // H가 z 또는 vz 한 state를 고르는 행벡터이므로 S=H P H^T+R는 scalar다.
    const double innovation_variance = covariance_[observed_state * 3U + observed_state] + variance;
    std::array<double, 3> gain{};
    for (std::size_t row = 0U; row < 3U; ++row) {
      gain[row] = covariance_[row * 3U + observed_state] / innovation_variance;
      state_[row] += gain[row] * residual;
    }

    // Joseph form P=(I-KH)P(I-KH)^T+KRK^T은 단순 (I-KH)P보다 대칭/PSD에 강하다.
    std::array<double, 9> a{};
    for (std::size_t row = 0U; row < 3U; ++row) {
      a[row * 3U + row] = 1.0;
      a[row * 3U + observed_state] -= gain[row];
    }
    std::array<double, 9> ap{};
    std::array<double, 9> corrected{};
    for (std::size_t row = 0U; row < 3U; ++row) {
      for (std::size_t column = 0U; column < 3U; ++column) {
        for (std::size_t inner = 0U; inner < 3U; ++inner) {
          ap[row * 3U + column] +=
            a[row * 3U + inner] * covariance_[inner * 3U + column];
        }
      }
    }
    for (std::size_t row = 0U; row < 3U; ++row) {
      for (std::size_t column = 0U; column < 3U; ++column) {
        for (std::size_t inner = 0U; inner < 3U; ++inner) {
          corrected[row * 3U + column] +=
            ap[row * 3U + inner] * a[column * 3U + inner];
        }
        corrected[row * 3U + column] += gain[row] * variance * gain[column];
      }
    }
    covariance_ = corrected;
  }

  void process_sample(const RuntimeSample & sample)
  {
    double dt_s = 0.005;
    if (last_stamp_ns_ > 0LL) {
      dt_s = static_cast<double>(sample.stamp_ns - last_stamp_ns_) * 1.0e-9;
      if (dt_s <= 0.0 || dt_s > 0.025) {
        invalid_samples_.fetch_add(1U, std::memory_order_relaxed);
        last_stamp_ns_ = sample.stamp_ns;
        return;
      }
    }
    last_stamp_ns_ = sample.stamp_ns;
    latest_stamp_ = sample.stamp;

    predict(sample.acceleration_mps2, dt_s);
    update_contact_state(sample.normal_force_n);

    // z=L cos(q), vz=-L sin(q) qdot가 관절 encoder를 몸체 수직 운동으로 바꾸는 FK/Jacobian이다.
    const double measured_height_m = kLegLengthM * std::cos(sample.joint_position_rad);
    const double measured_velocity_mps =
      -kLegLengthM * std::sin(sample.joint_position_rad) * sample.joint_velocity_rad_s;

    // force가 OFF threshold 아래로 들어온 즉시 제약을 끄고, 3표본 dwell은 상태 전이 확정에만 쓴다.
    const bool contact_constraint_active =
      contact_state_ == ContactState::kStance && low_force_count_ == 0U;
    if (contact_constraint_active && !filter_initialized_) {
      state_[0] = measured_height_m;
      state_[1] = measured_velocity_mps;
      covariance_[0] = 0.006 * 0.006;
      covariance_[4] = 0.03 * 0.03;
      filter_initialized_ = true;
      ++contact_updates_;
    } else if (contact_constraint_active) {
      constexpr double height_variance = 0.006 * 0.006;
      constexpr double velocity_variance = 0.03 * 0.03;
      const double height_residual = measured_height_m - state_[0];
      const double velocity_residual = measured_velocity_mps - state_[1];
      // NIS는 residual을 예상 분산으로 정규화한다. 큰 값은 slip/outlier 가능성을 뜻한다.
      latest_nis_ =
        height_residual * height_residual / (covariance_[0] + height_variance) +
        velocity_residual * velocity_residual / (covariance_[4] + velocity_variance);
      if (latest_nis_ <= kNisGate2D) {
        scalar_update(0U, height_residual, height_variance);
        // 첫 update가 state를 바꿨으므로 속도 residual을 현재 state 기준으로 다시 계산한다.
        scalar_update(1U, measured_velocity_mps - state_[1], velocity_variance);
        ++contact_updates_;
      } else {
        ++rejected_updates_;
      }
    }
    ++processed_samples_;
  }

  void publish_outputs()
  {
    geometry_msgs::msg::PoseWithCovarianceStamped estimate;
    estimate.header.stamp = latest_stamp_;
    estimate.header.frame_id = "map";
    estimate.pose.pose.position.z = state_[0];
    estimate.pose.pose.orientation.w = 1.0;
    // Pose covariance는 [x,y,z,roll,pitch,yaw] 6x6 row-major이며 z diagonal index는 14다.
    std::fill(estimate.pose.covariance.begin(), estimate.pose.covariance.end(), 0.0);
    estimate.pose.covariance[0] = 1.0e3;
    estimate.pose.covariance[7] = 1.0e3;
    estimate.pose.covariance[14] = covariance_[0];
    estimate.pose.covariance[21] = 1.0e3;
    estimate.pose.covariance[28] = 1.0e3;
    estimate.pose.covariance[35] = 1.0e3;
    pose_pub_->publish(estimate);

    daily_robotics_2026_10_21::msg::EstimatorStats stats;
    stats.header.stamp = latest_stamp_;
    stats.header.frame_id = "base_link";
    stats.processed_samples = processed_samples_;
    stats.contact_updates = contact_updates_;
    stats.rejected_updates = rejected_updates_;
    stats.contact_transitions = contact_transitions_;
    stats.queue_drops = queue_drops_.load(std::memory_order_relaxed);
    stats.invalid_samples = invalid_samples_.load(std::memory_order_relaxed);
    stats.max_samples_per_cycle = max_samples_per_cycle_;
    stats.contact_state = static_cast<std::uint8_t>(contact_state_);
    stats.innovation_nis = latest_nis_;
    stats.height_variance = covariance_[0];
    stats.accel_bias_mps2 = state_[2];
    stats.max_cycle_time_us = max_cycle_time_us_;
    stats.bounded_contract_ok =
      stats.queue_drops == 0U && stats.invalid_samples == 0U &&
      stats.max_samples_per_cycle <= static_cast<std::uint32_t>(kMaxSamplesPerCycle);
    stats_pub_->publish(stats);
  }

  void on_estimator_cycle()
  {
    const auto cycle_start = std::chrono::steady_clock::now();
    RuntimeSample sample;
    std::size_t drained = 0U;
    // backlog가 커져도 callback 하나가 최대 네 표본만 처리해 다른 callback을 무한히 굶기지 않는다.
    while (drained < kMaxSamplesPerCycle && sample_queue_.pop(sample)) {
      process_sample(sample);
      ++drained;
    }
    max_samples_per_cycle_ = std::max(
      max_samples_per_cycle_, static_cast<std::uint32_t>(drained));

    if (drained > 0U) {
      const auto elapsed = std::chrono::steady_clock::now() - cycle_start;
      const double elapsed_us =
        std::chrono::duration<double, std::micro>(elapsed).count();
      max_cycle_time_us_ = std::max(max_cycle_time_us_, elapsed_us);
      publish_outputs();
    }
  }

  SampleSpscQueue sample_queue_;
  std::atomic<std::uint32_t> queue_drops_{0U};
  std::atomic<std::uint32_t> invalid_samples_{0U};
  std::array<double, 3> state_{};
  std::array<double, 9> covariance_{};
  ContactState contact_state_{ContactState::kAir};
  std::uint32_t high_force_count_{0U};
  std::uint32_t low_force_count_{0U};
  std::uint32_t processed_samples_{0U};
  std::uint32_t contact_updates_{0U};
  std::uint32_t rejected_updates_{0U};
  std::uint32_t contact_transitions_{0U};
  std::uint32_t max_samples_per_cycle_{0U};
  std::int64_t last_stamp_ns_{0LL};
  builtin_interfaces::msg::Time latest_stamp_{};
  double latest_nis_{0.0};
  double max_cycle_time_us_{0.0};
  bool filter_initialized_{false};

  rclcpp::CallbackGroup::SharedPtr input_group_;
  rclcpp::CallbackGroup::SharedPtr estimator_group_;
  rclcpp::Subscription<daily_robotics_2026_10_21::msg::ContactSample>::SharedPtr sample_sub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pose_pub_;
  rclcpp::Publisher<daily_robotics_2026_10_21::msg::EstimatorStats>::SharedPtr stats_pub_;
  rclcpp::TimerBase::SharedPtr estimator_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<BoundedContactEkf>();
  // 두 worker는 입력 producer와 estimator consumer를 병렬 실행할 수 있게 한다.
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2U);
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
