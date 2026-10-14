#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "daily_robotics_2026_10_14/msg/estimator_status.hpp"
#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"

namespace
{
constexpr std::size_t kHistoryCapacity = 64U;
constexpr double kAccelerationNoise = 0.35;       // 연속 백색 가속도 process noise 밀도
constexpr double kVelocityVariance = 0.025 * 0.025;
constexpr double kPositionVariance = 0.035 * 0.035;

struct FilterState
{
  double position{0.0};
  double velocity{0.0};
  double p00{4.0};
  double p01{0.0};
  double p10{0.0};
  double p11{1.0};
};

// constant-velocity 모델 x' = F x와 white-acceleration Q를 한 step 적용한다.
void predict(FilterState & state, const double dt)
{
  // [p, v]^T <- [[1,dt],[0,1]] [p,v]^T, 즉 p <- p + v*dt다.
  state.position += state.velocity * dt;

  const double dt2 = dt * dt;
  const double dt3 = dt2 * dt;
  const double old_p00 = state.p00;
  const double old_p01 = state.p01;
  const double old_p10 = state.p10;
  const double old_p11 = state.p11;

  // P <- F P F^T + Q. Q는 연속 white acceleration을 dt 구간에 적분한 2x2 covariance다.
  state.p00 = old_p00 + dt * (old_p01 + old_p10) + dt2 * old_p11 +
    kAccelerationNoise * dt3 / 3.0;
  state.p01 = old_p01 + dt * old_p11 + kAccelerationNoise * dt2 / 2.0;
  state.p10 = old_p10 + dt * old_p11 + kAccelerationNoise * dt2 / 2.0;
  state.p11 = old_p11 + kAccelerationNoise * dt;
}

// scalar z = Hx + noise에 대한 Joseph-form Kalman update다.
// h0/h1을 바꾸면 같은 함수로 position H=[1,0]과 velocity H=[0,1]를 처리할 수 있다.
double scalar_update(
  FilterState & state, const double measurement, const double h0, const double h1,
  const double measurement_variance)
{
  const double innovation = measurement - (h0 * state.position + h1 * state.velocity);
  const double innovation_variance =
    h0 * (h0 * state.p00 + h1 * state.p01) +
    h1 * (h0 * state.p10 + h1 * state.p11) + measurement_variance;

  if (!(innovation_variance > std::numeric_limits<double>::epsilon()) ||
    !std::isfinite(innovation_variance))
  {
    return innovation;
  }

  const double gain0 = (state.p00 * h0 + state.p01 * h1) / innovation_variance;
  const double gain1 = (state.p10 * h0 + state.p11 * h1) / innovation_variance;
  state.position += gain0 * innovation;
  state.velocity += gain1 * innovation;

  // A = I-KH를 만든 뒤 P <- A P A^T + K R K^T를 계산한다.
  // 단순 P <- (I-KH)P보다 Joseph form이 유한 정밀도에서 대칭성과 양의 준정부호를 잘 보존한다.
  const double a00 = 1.0 - gain0 * h0;
  const double a01 = -gain0 * h1;
  const double a10 = -gain1 * h0;
  const double a11 = 1.0 - gain1 * h1;

  const double ap00 = a00 * state.p00 + a01 * state.p10;
  const double ap01 = a00 * state.p01 + a01 * state.p11;
  const double ap10 = a10 * state.p00 + a11 * state.p10;
  const double ap11 = a10 * state.p01 + a11 * state.p11;

  const double new_p00 = ap00 * a00 + ap01 * a01 + gain0 * gain0 * measurement_variance;
  const double new_p01 = ap00 * a10 + ap01 * a11 + gain0 * gain1 * measurement_variance;
  const double new_p10 = ap10 * a00 + ap11 * a01 + gain1 * gain0 * measurement_variance;
  const double new_p11 = ap10 * a10 + ap11 * a11 + gain1 * gain1 * measurement_variance;
  const double symmetric_off_diagonal = 0.5 * (new_p01 + new_p10);

  state.p00 = new_p00;
  state.p01 = symmetric_off_diagonal;
  state.p10 = symmetric_off_diagonal;
  state.p11 = new_p11;
  return innovation;
}

void update_velocity(FilterState & state, const double measured_velocity)
{
  (void)scalar_update(state, measured_velocity, 0.0, 1.0, kVelocityVariance);
}

double update_position(FilterState & state, const double measured_position)
{
  return scalar_update(state, measured_position, 1.0, 0.0, kPositionVariance);
}
}  // namespace

class BoundedOosmEstimator final : public rclcpp::Node
{
public:
  BoundedOosmEstimator()
  : Node("bounded_oosm_estimator")
  {
    const auto sensor_qos = rclcpp::SensorDataQoS().keep_last(16);

    // std::bind는 멤버 함수와 this를 묶어 ROS callback callable로 만드는 C++ 문법이다.
    velocity_sub_ = create_subscription<geometry_msgs::msg::TwistStamped>(
      "/study/wheel_velocity", sensor_qos,
      std::bind(&BoundedOosmEstimator::on_velocity, this, std::placeholders::_1));
    delayed_position_sub_ = create_subscription<geometry_msgs::msg::PointStamped>(
      "/study/delayed_position", sensor_qos,
      std::bind(&BoundedOosmEstimator::on_delayed_position, this, std::placeholders::_1));

    odometry_pub_ = create_publisher<nav_msgs::msg::Odometry>(
      "/study/oosm_odometry", sensor_qos);
    status_pub_ = create_publisher<daily_robotics_2026_10_14::msg::EstimatorStatus>(
      "/study/estimator_status", sensor_qos);

    RCLCPP_INFO(
      get_logger(), "fixed history=%zu states; numerical replay is bounded",
      kHistoryCapacity);
  }

private:
  struct Snapshot
  {
    std::int64_t stamp_ns{0};
    double measured_velocity{0.0};
    FilterState state{};
  };

  // 속도 callback은 새 현재 상태를 예측/보정하고, OOSM 재전파에 필요한 snapshot을 저장한다.
  void on_velocity(const geometry_msgs::msg::TwistStamped::SharedPtr message)
  {
    // rclcpp::Time은 builtin_interfaces/Time을 nanosecond 정수 시각으로 바꾸어 안전하게 비교하게 한다.
    const rclcpp::Time stamp(message->header.stamp);
    const double measured_velocity = message->twist.linear.x;

    if (!initialized_) {
      current_stamp_ = stamp;
      update_velocity(replay_filter_, measured_velocity);
      update_velocity(naive_filter_, measured_velocity);
      initialized_ = true;
    } else {
      const double dt = (stamp - current_stamp_).seconds();
      // timestamp 역행이나 지나치게 큰 gap에서 잘못된 covariance를 만들지 않고 sample을 거부한다.
      if (!(dt > 0.0 && dt <= 1.0)) {
        ++invalid_time_samples_;
        RCLCPP_WARN_THROTTLE(
          get_logger(), *get_clock(), 1000, "invalid velocity dt=%.6f; sample ignored", dt);
        return;
      }

      predict(replay_filter_, dt);
      update_velocity(replay_filter_, measured_velocity);
      predict(naive_filter_, dt);
      update_velocity(naive_filter_, measured_velocity);
      current_stamp_ = stamp;
    }

    push_snapshot(Snapshot{stamp.nanoseconds(), measured_velocity, replay_filter_});
    publish_outputs(stamp);
  }

  // 지연 위치 callback은 measurement stamp의 과거 상태를 보정하고 저장 입력을 현재까지 재생한다.
  void on_delayed_position(const geometry_msgs::msg::PointStamped::SharedPtr message)
  {
    if (!initialized_ || history_size_ == 0U) {
      return;
    }

    const auto replay_start = std::chrono::steady_clock::now();
    const rclcpp::Time measurement_stamp(message->header.stamp);
    last_measurement_age_ms_ = (current_stamp_ - measurement_stamp).seconds() * 1000.0;

    // 비교용 naive filter는 과거 위치를 현재 위치라고 잘못 간주한다.
    // 이 의도적인 anti-pattern 덕분에 timestamp-correct replay의 효과를 같은 입력으로 비교할 수 있다.
    (void)update_position(naive_filter_, message->point.x);

    std::size_t corrected_index = 0U;
    if (!find_snapshot_at_or_before(measurement_stamp.nanoseconds(), corrected_index)) {
      ++dropped_too_old_measurements_;
      last_replay_steps_ = 0U;
      return;
    }

    Snapshot & corrected = logical_snapshot(corrected_index);
    last_innovation_ = update_position(corrected.state, message->point.x);

    // 과거 보정 뒤 j+1 ... latest의 저장된 속도 입력을 다시 predict/update한다.
    // loop 상한은 history_size-1, 절대 kHistoryCapacity-1을 넘지 않는다.
    FilterState propagated = corrected.state;
    std::int64_t previous_stamp_ns = corrected.stamp_ns;
    for (std::size_t index = corrected_index + 1U; index < history_size_; ++index) {
      Snapshot & next = logical_snapshot(index);
      const double dt = static_cast<double>(next.stamp_ns - previous_stamp_ns) * 1.0e-9;
      predict(propagated, dt);
      update_velocity(propagated, next.measured_velocity);
      next.state = propagated;
      previous_stamp_ns = next.stamp_ns;
    }

    replay_filter_ = logical_snapshot(history_size_ - 1U).state;
    last_replay_steps_ = static_cast<std::uint32_t>(history_size_ - 1U - corrected_index);
    max_replay_steps_ = std::max(max_replay_steps_, last_replay_steps_);
    ++accepted_delayed_measurements_;

    // steady_clock은 ROS time pause/jump와 무관한 단조 시계이므로 callback 실행시간 계측에 사용한다.
    const auto elapsed = std::chrono::steady_clock::now() - replay_start;
    const double elapsed_us =
      std::chrono::duration<double, std::micro>(elapsed).count();
    max_replay_time_us_ = std::max(max_replay_time_us_, elapsed_us);
  }

  // 순환 버퍼의 논리 index(오래된 것부터 0)를 실제 std::array index로 매핑한다.
  Snapshot & logical_snapshot(const std::size_t logical_index)
  {
    return history_[(history_head_ + logical_index) % kHistoryCapacity];
  }

  const Snapshot & logical_snapshot(const std::size_t logical_index) const
  {
    return history_[(history_head_ + logical_index) % kHistoryCapacity];
  }

  // full이면 가장 오래된 한 칸을 버리고 head를 전진한다. heap allocation은 일어나지 않는다.
  void push_snapshot(const Snapshot & snapshot)
  {
    if (history_size_ < kHistoryCapacity) {
      const std::size_t insert_index = (history_head_ + history_size_) % kHistoryCapacity;
      history_[insert_index] = snapshot;
      ++history_size_;
      return;
    }

    history_head_ = (history_head_ + 1U) % kHistoryCapacity;
    const std::size_t insert_index = (history_head_ + history_size_ - 1U) % kHistoryCapacity;
    history_[insert_index] = snapshot;
  }

  // 측정 시각 이하인 가장 최신 snapshot을 찾는다. search 역시 최대 64회로 제한된다.
  bool find_snapshot_at_or_before(
    const std::int64_t measurement_ns, std::size_t & found_index) const
  {
    if (history_size_ == 0U || measurement_ns < logical_snapshot(0U).stamp_ns ||
      measurement_ns > logical_snapshot(history_size_ - 1U).stamp_ns)
    {
      return false;
    }

    found_index = 0U;
    for (std::size_t index = 1U; index < history_size_; ++index) {
      if (logical_snapshot(index).stamp_ns > measurement_ns) {
        break;
      }
      found_index = index;
    }
    return true;
  }

  // 표준 Odometry와 교육용 진단 message를 같은 현재 stamp로 내보낸다.
  void publish_outputs(const rclcpp::Time & stamp)
  {
    nav_msgs::msg::Odometry odometry;
    odometry.header.stamp = stamp;
    odometry.header.frame_id = "map";
    odometry.child_frame_id = "base_link";
    odometry.pose.pose.position.x = replay_filter_.position;
    odometry.pose.pose.orientation.w = 1.0;
    odometry.pose.covariance[0] = replay_filter_.p00;
    odometry.twist.twist.linear.x = replay_filter_.velocity;
    odometry.twist.covariance[0] = replay_filter_.p11;
    odometry_pub_->publish(odometry);

    daily_robotics_2026_10_14::msg::EstimatorStatus status;
    status.header.stamp = stamp;
    status.header.frame_id = "map";
    status.replay_position = replay_filter_.position;
    status.naive_position = naive_filter_.position;
    status.replay_velocity = replay_filter_.velocity;
    status.naive_velocity = naive_filter_.velocity;
    status.buffered_states = static_cast<std::uint32_t>(history_size_);
    status.last_replay_steps = last_replay_steps_;
    status.accepted_delayed_measurements = accepted_delayed_measurements_;
    status.dropped_too_old_measurements = dropped_too_old_measurements_;
    status.last_measurement_age_ms = last_measurement_age_ms_;
    status.max_replay_time_us = max_replay_time_us_;
    status_pub_->publish(status);
  }

  std::array<Snapshot, kHistoryCapacity> history_{};
  std::size_t history_head_{0U};
  std::size_t history_size_{0U};
  FilterState replay_filter_{};
  FilterState naive_filter_{};
  rclcpp::Time current_stamp_{0, 0, RCL_ROS_TIME};
  bool initialized_{false};

  std::uint64_t accepted_delayed_measurements_{0U};
  std::uint64_t dropped_too_old_measurements_{0U};
  std::uint64_t invalid_time_samples_{0U};
  std::uint32_t last_replay_steps_{0U};
  std::uint32_t max_replay_steps_{0U};
  double last_measurement_age_ms_{0.0};
  double max_replay_time_us_{0.0};
  double last_innovation_{0.0};

  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr velocity_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr delayed_position_sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odometry_pub_;
  rclcpp::Publisher<daily_robotics_2026_10_14::msg::EstimatorStatus>::SharedPtr status_pub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor의 callback 직렬화가 history의 단일 소유자라는 전제다.
  // MultiThreadedExecutor로 바꾸면 callback group/lock-free handoff 등 동시성 설계를 추가해야 한다.
  rclcpp::spin(std::make_shared<BoundedOosmEstimator>());
  rclcpp::shutdown();
  return 0;
}
