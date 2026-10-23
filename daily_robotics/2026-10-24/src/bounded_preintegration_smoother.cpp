#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "daily_robotics_2026_10_24/msg/preintegration_stats.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr std::size_t kStateDim = 6U;       // [x, y, vx, vy, yaw, gyro_bias]
constexpr std::size_t kMaxStates = 6U;
constexpr std::size_t kMaxDim = kStateDim * kMaxStates;
constexpr std::size_t kPriorDim = kStateDim * (kMaxStates - 1U);
constexpr std::size_t kImuQueueSize = 65U;  // ring은 한 칸을 비우므로 실제 수용량은 64
constexpr std::size_t kVisualQueueSize = 9U;
constexpr std::size_t kStagedImuCapacity = 32U;
constexpr std::size_t kMaxImuDrainPerCycle = 8U;
constexpr std::size_t kSolverIterations = 3U;
constexpr double kBiasJacobianEpsilon = 1.0e-5;

using Vector = std::array<double, kMaxDim>;
using Matrix = std::array<std::array<double, kMaxDim>, kMaxDim>;

double wrap_angle(double angle)
{
  return std::atan2(std::sin(angle), std::cos(angle));
}

double yaw_from_quaternion(const geometry_msgs::msg::Quaternion & q)
{
  // 평면 quaternion의 yaw = atan2(2(wz+xy), 1-2(y^2+z^2))를 그대로 계산한다.
  const double sin_yaw = 2.0 * (q.w * q.z + q.x * q.y);
  const double cos_yaw = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
  return std::atan2(sin_yaw, cos_yaw);
}

void set_yaw(double yaw, geometry_msgs::msg::Quaternion & q)
{
  q.x = 0.0;
  q.y = 0.0;
  q.z = std::sin(0.5 * yaw);
  q.w = std::cos(0.5 * yaw);
}

double clamp_step(double value, double limit)
{
  return std::max(-limit, std::min(limit, value));
}

template<typename T, std::size_t Capacity>
class SpscRing
{
public:
  bool push(const T & value)
  {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1U) % Capacity;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    data_[head] = value;
    // release는 data 쓰기가 head 공개보다 먼저 다른 thread에 보이도록 한다.
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T & value)
  {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    value = data_[tail];
    tail_.store((tail + 1U) % Capacity, std::memory_order_release);
    return true;
  }

private:
  std::array<T, Capacity> data_{};
  alignas(64) std::atomic<std::size_t> head_{0U};
  alignas(64) std::atomic<std::size_t> tail_{0U};
};

struct ImuSample
{
  std::int64_t stamp_ns{0};
  double ax{0.0};
  double ay{0.0};
  double wz{0.0};
};

struct VisualSample
{
  std::int64_t stamp_ns{0};
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
  double var_x{4.0e-4};
  double var_y{4.0e-4};
  double var_yaw{1.0e-4};
};

struct Delta2d
{
  double theta{0.0};
  double vx{0.0};
  double vy{0.0};
  double px{0.0};
  double py{0.0};
};

struct PreintegratedFactor
{
  double dt{0.0};
  double bias_reference{0.0};
  Delta2d delta{};
  double j_theta_bias{0.0};
  double j_vx_bias{0.0};
  double j_vy_bias{0.0};
  double j_px_bias{0.0};
  double j_py_bias{0.0};
  std::uint32_t sample_count{0U};
};

struct Preintegrator
{
  void reset(std::int64_t stamp_ns, double bias)
  {
    last_stamp_ns = stamp_ns;
    bias_reference = bias;
    nominal = Delta2d{};
    perturbed = Delta2d{};
    total_dt = 0.0;
    sample_count = 0U;
  }

  static void integrate_one(Delta2d & value, const ImuSample & sample, double dt, double bias)
  {
    const double unbiased_wz = sample.wz - bias;
    const double midpoint_theta = value.theta + 0.5 * unbiased_wz * dt;
    const double c = std::cos(midpoint_theta);
    const double s = std::sin(midpoint_theta);

    // Δv = Σ ΔR_k a_k dt, Δp = Σ(Δv_k dt + 1/2 ΔR_k a_k dt²).
    // local keyframe 좌표계에서 적분하므로 keyframe의 세계 자세가 바뀌어도 재적분할 필요가 없다.
    const double local_ax = c * sample.ax - s * sample.ay;
    const double local_ay = s * sample.ax + c * sample.ay;
    value.px += value.vx * dt + 0.5 * local_ax * dt * dt;
    value.py += value.vy * dt + 0.5 * local_ay * dt * dt;
    value.vx += local_ax * dt;
    value.vy += local_ay * dt;
    value.theta = wrap_angle(value.theta + unbiased_wz * dt);
  }

  bool add(const ImuSample & sample)
  {
    if (sample.stamp_ns <= last_stamp_ns) {
      return false;
    }
    const double dt = static_cast<double>(sample.stamp_ns - last_stamp_ns) * 1.0e-9;
    if (!(dt > 0.0 && dt <= 0.020)) {
      last_stamp_ns = sample.stamp_ns;
      return false;
    }
    integrate_one(nominal, sample, dt, bias_reference);
    integrate_one(perturbed, sample, dt, bias_reference + kBiasJacobianEpsilon);
    total_dt += dt;
    last_stamp_ns = sample.stamp_ns;
    ++sample_count;
    return true;
  }

  PreintegratedFactor factor() const
  {
    PreintegratedFactor result;
    result.dt = total_dt;
    result.bias_reference = bias_reference;
    result.delta = nominal;
    // J_bias ≈ (Δ(b+epsilon)-Δ(b))/epsilon: 늦은 bias 수정에 쓰는 bounded finite difference다.
    result.j_theta_bias = (perturbed.theta - nominal.theta) / kBiasJacobianEpsilon;
    result.j_vx_bias = (perturbed.vx - nominal.vx) / kBiasJacobianEpsilon;
    result.j_vy_bias = (perturbed.vy - nominal.vy) / kBiasJacobianEpsilon;
    result.j_px_bias = (perturbed.px - nominal.px) / kBiasJacobianEpsilon;
    result.j_py_bias = (perturbed.py - nominal.py) / kBiasJacobianEpsilon;
    result.sample_count = sample_count;
    return result;
  }

  std::int64_t last_stamp_ns{0};
  double bias_reference{0.0};
  Delta2d nominal{};
  Delta2d perturbed{};
  double total_dt{0.0};
  std::uint32_t sample_count{0U};
};

struct State
{
  std::int64_t stamp_ns{0};
  double x{0.0};
  double y{0.0};
  double vx{0.0};
  double vy{0.0};
  double yaw{0.0};
  double gyro_bias{0.0};
  VisualSample visual{};
  PreintegratedFactor from_previous{};
};

double & component(State & state, std::size_t index)
{
  switch (index) {
    case 0U: return state.x;
    case 1U: return state.y;
    case 2U: return state.vx;
    case 3U: return state.vy;
    case 4U: return state.yaw;
    default: return state.gyro_bias;
  }
}

double component(const State & state, std::size_t index)
{
  switch (index) {
    case 0U: return state.x;
    case 1U: return state.y;
    case 2U: return state.vx;
    case 3U: return state.vy;
    case 4U: return state.yaw;
    default: return state.gyro_bias;
  }
}
}  // namespace

class BoundedPreintegrationSmoother : public rclcpp::Node
{
public:
  BoundedPreintegrationSmoother()
  : Node("bounded_preintegration_smoother")
  {
    // callback group을 분리하면 MultiThreadedExecutor에서 두 producer와 worker의 책임 경계가 명확해진다.
    imu_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    visual_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    worker_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions imu_options;
    imu_options.callback_group = imu_group_;
    imu_subscription_ = create_subscription<sensor_msgs::msg::Imu>(
      "/sim/imu", rclcpp::SensorDataQoS(),
      std::bind(&BoundedPreintegrationSmoother::on_imu, this, std::placeholders::_1),
      imu_options);

    rclcpp::SubscriptionOptions visual_options;
    visual_options.callback_group = visual_group_;
    visual_subscription_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/sim/visual_pose", rclcpp::QoS(rclcpp::KeepLast(20)).reliable(),
      std::bind(&BoundedPreintegrationSmoother::on_visual, this, std::placeholders::_1),
      visual_options);

    odometry_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/vio/odometry", rclcpp::QoS(rclcpp::KeepLast(10)).reliable());
    stats_publisher_ = create_publisher<daily_robotics_2026_10_24::msg::PreintegrationStats>(
      "/vio/stats", rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    // 400 Hz worker는 한 번에 IMU 최대 8개와 visual 최대 1개만 처리해 callback work를 제한한다.
    worker_timer_ = create_wall_timer(
      2500us, std::bind(&BoundedPreintegrationSmoother::process_queues, this), worker_group_);
  }

private:
  void on_imu(const sensor_msgs::msg::Imu::SharedPtr message)
  {
    if (message->header.frame_id != "imu_link" ||
      !std::isfinite(message->linear_acceleration.x) ||
      !std::isfinite(message->linear_acceleration.y) ||
      !std::isfinite(message->angular_velocity.z))
    {
      imu_queue_drops_.fetch_add(1U, std::memory_order_relaxed);
      return;
    }
    ImuSample sample;
    sample.stamp_ns = rclcpp::Time(message->header.stamp).nanoseconds();
    sample.ax = message->linear_acceleration.x;
    sample.ay = message->linear_acceleration.y;
    sample.wz = message->angular_velocity.z;
    if (!imu_queue_.push(sample)) {
      imu_queue_drops_.fetch_add(1U, std::memory_order_relaxed);
    }
  }

  void on_visual(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr message)
  {
    if (message->header.frame_id != "map") {
      visual_queue_drops_.fetch_add(1U, std::memory_order_relaxed);
      return;
    }
    VisualSample sample;
    sample.stamp_ns = rclcpp::Time(message->header.stamp).nanoseconds();
    sample.x = message->pose.pose.position.x;
    sample.y = message->pose.pose.position.y;
    sample.yaw = yaw_from_quaternion(message->pose.pose.orientation);
    sample.var_x = std::max(1.0e-6, message->pose.covariance[0]);
    sample.var_y = std::max(1.0e-6, message->pose.covariance[7]);
    sample.var_yaw = std::max(1.0e-6, message->pose.covariance[35]);
    if (!visual_queue_.push(sample)) {
      visual_queue_drops_.fetch_add(1U, std::memory_order_relaxed);
    }
  }

  void process_queues()
  {
    std::size_t drained = 0U;
    ImuSample imu;
    while (drained < kMaxImuDrainPerCycle && imu_queue_.pop(imu)) {
      if (staged_imu_count_ < kStagedImuCapacity) {
        staged_imus_[staged_imu_count_] = imu;
        ++staged_imu_count_;
      } else {
        ++staged_imu_drops_;
      }
      ++drained;
    }
    max_imu_batch_ = std::max(max_imu_batch_, static_cast<std::uint32_t>(drained));

    if (!has_pending_visual_) {
      has_pending_visual_ = visual_queue_.pop(pending_visual_);
    }
    if (!has_pending_visual_ || staged_imu_count_ == 0U) {
      return;
    }

    // future IMU를 현재 keyframe factor에 섞지 않는다. 마지막 staged stamp가 도달할 때까지 visual을 보류한다.
    if (staged_imus_[staged_imu_count_ - 1U].stamp_ns < pending_visual_.stamp_ns) {
      return;
    }

    std::size_t consumed = 0U;
    while (consumed < staged_imu_count_ &&
      staged_imus_[consumed].stamp_ns <= pending_visual_.stamp_ns)
    {
      if (initialized_ && staged_imus_[consumed].stamp_ns > last_keyframe_stamp_ns_) {
        if (preintegrator_.add(staged_imus_[consumed])) {
          ++imu_samples_total_;
        }
      }
      ++consumed;
    }
    for (std::size_t index = consumed; index < staged_imu_count_; ++index) {
      staged_imus_[index - consumed] = staged_imus_[index];
    }
    staged_imu_count_ -= consumed;

    process_visual(pending_visual_);
    has_pending_visual_ = false;
  }

  void process_visual(const VisualSample & visual)
  {
    if (initialized_ && visual.stamp_ns <= last_keyframe_stamp_ns_) {
      ++stale_visual_drops_;
      RCLCPP_WARN(
        get_logger(), "visual stamp is not newer: current=%ld last=%ld",
        static_cast<long>(visual.stamp_ns), static_cast<long>(last_keyframe_stamp_ns_));
      return;
    }

    const auto started = std::chrono::steady_clock::now();
    std::uint32_t interval_samples = 0U;
    double interval_seconds = 0.0;

    if (!initialized_) {
      State initial;
      initial.stamp_ns = visual.stamp_ns;
      initial.x = visual.x;
      initial.y = visual.y;
      initial.yaw = visual.yaw;
      initial.visual = visual;
      states_[0] = initial;
      state_count_ = 1U;
      initialized_ = true;
    } else {
      const PreintegratedFactor factor = preintegrator_.factor();
      interval_samples = factor.sample_count;
      interval_seconds = factor.dt;
      if (factor.sample_count == 0U || !(factor.dt > 0.0)) {
        ++stale_visual_drops_;
        RCLCPP_WARN(
          get_logger(), "visual interval has no usable IMU: stamp=%ld last=%ld samples=%u dt=%.9f",
          static_cast<long>(visual.stamp_ns), static_cast<long>(last_keyframe_stamp_ns_),
          factor.sample_count, factor.dt);
        preintegrator_.reset(visual.stamp_ns, states_[state_count_ - 1U].gyro_bias);
        last_keyframe_stamp_ns_ = visual.stamp_ns;
        return;
      }

      const State & previous = states_[state_count_ - 1U];
      State next;
      next.stamp_ns = visual.stamp_ns;
      next.visual = visual;
      next.from_previous = factor;

      const double bias_delta = previous.gyro_bias - factor.bias_reference;
      const double dtheta = factor.delta.theta + factor.j_theta_bias * bias_delta;
      const double dvx = factor.delta.vx + factor.j_vx_bias * bias_delta;
      const double dvy = factor.delta.vy + factor.j_vy_bias * bias_delta;
      const double dpx = factor.delta.px + factor.j_px_bias * bias_delta;
      const double dpy = factor.delta.py + factor.j_py_bias * bias_delta;
      const double c = std::cos(previous.yaw);
      const double s = std::sin(previous.yaw);

      // p_j=p_i+v_i*dt+R_i*Δp, v_j=v_i+R_i*Δv, R_j=R_i*ΔR의 예측식이다.
      next.x = previous.x + previous.vx * factor.dt + c * dpx - s * dpy;
      next.y = previous.y + previous.vy * factor.dt + s * dpx + c * dpy;
      next.vx = previous.vx + c * dvx - s * dvy;
      next.vy = previous.vy + s * dvx + c * dvy;
      next.yaw = wrap_angle(previous.yaw + dtheta);
      next.gyro_bias = previous.gyro_bias;
      states_[state_count_] = next;
      ++state_count_;

      if (!solve_window()) {
        ++solve_failures_;
      }
      if (state_count_ == kMaxStates) {
        if (!marginalize_oldest()) {
          ++solve_failures_;
        } else {
          ++marginalizations_;
        }
      }
    }

    last_keyframe_stamp_ns_ = visual.stamp_ns;
    preintegrator_.reset(visual.stamp_ns, states_[state_count_ - 1U].gyro_bias);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const double elapsed_us =
      std::chrono::duration<double, std::micro>(elapsed).count();
    max_kernel_us_ = std::max(max_kernel_us_, elapsed_us);
    publish_result(visual, interval_samples, interval_seconds, elapsed_us);
  }

  static void imu_residual(
    const State & left, const State & right, const PreintegratedFactor & factor,
    std::array<double, kStateDim> & residual)
  {
    const double c = std::cos(left.yaw);
    const double s = std::sin(left.yaw);
    const double world_px = right.x - left.x - left.vx * factor.dt;
    const double world_py = right.y - left.y - left.vy * factor.dt;
    const double world_vx = right.vx - left.vx;
    const double world_vy = right.vy - left.vy;
    const double bias_delta = left.gyro_bias - factor.bias_reference;

    // R_i^T(p_j-p_i-v_iΔt)-Δp(b_i)와 R_i^T(v_j-v_i)-Δv(b_i)가 IMU factor 잔차다.
    residual[0] = c * world_px + s * world_py -
      (factor.delta.px + factor.j_px_bias * bias_delta);
    residual[1] = -s * world_px + c * world_py -
      (factor.delta.py + factor.j_py_bias * bias_delta);
    residual[2] = c * world_vx + s * world_vy -
      (factor.delta.vx + factor.j_vx_bias * bias_delta);
    residual[3] = -s * world_vx + c * world_vy -
      (factor.delta.vy + factor.j_vy_bias * bias_delta);
    residual[4] = wrap_angle(
      (right.yaw - left.yaw) -
      (factor.delta.theta + factor.j_theta_bias * bias_delta));
    residual[5] = right.gyro_bias - left.gyro_bias;
  }

  void clear_system(std::size_t dimension)
  {
    for (std::size_t row = 0U; row < dimension; ++row) {
      gradient_[row] = 0.0;
      for (std::size_t column = 0U; column < dimension; ++column) {
        hessian_[row][column] = 0.0;
      }
    }
  }

  void add_imu_factor(std::size_t left_index, std::size_t right_index)
  {
    const State & left = states_[left_index];
    const State & right = states_[right_index];
    const PreintegratedFactor & factor = right.from_previous;
    std::array<double, kStateDim> residual{};
    imu_residual(left, right, factor, residual);

    // 교육 코드에서는 analytic Jacobian 대신 고정 12열 central difference를 사용한다.
    // 연산량은 factor당 정확히 24회 residual 평가로 제한되지만, 제품 구현은 analytic/automatic Jacobian을 권한다.
    std::array<std::array<double, 2U * kStateDim>, kStateDim> jacobian{};
    for (std::size_t column = 0U; column < 2U * kStateDim; ++column) {
      State plus_left = left;
      State minus_left = left;
      State plus_right = right;
      State minus_right = right;
      const std::size_t local = column % kStateDim;
      const double epsilon = (local == 5U) ? 1.0e-7 : 1.0e-6;
      if (column < kStateDim) {
        component(plus_left, local) += epsilon;
        component(minus_left, local) -= epsilon;
      } else {
        component(plus_right, local) += epsilon;
        component(minus_right, local) -= epsilon;
      }
      plus_left.yaw = wrap_angle(plus_left.yaw);
      minus_left.yaw = wrap_angle(minus_left.yaw);
      plus_right.yaw = wrap_angle(plus_right.yaw);
      minus_right.yaw = wrap_angle(minus_right.yaw);
      std::array<double, kStateDim> plus_residual{};
      std::array<double, kStateDim> minus_residual{};
      imu_residual(plus_left, plus_right, factor, plus_residual);
      imu_residual(minus_left, minus_right, factor, minus_residual);
      for (std::size_t row = 0U; row < kStateDim; ++row) {
        double difference = plus_residual[row] - minus_residual[row];
        if (row == 4U) {
          difference = wrap_angle(difference);
        }
        jacobian[row][column] = difference / (2.0 * epsilon);
      }
    }

    const std::array<double, kStateDim> weights{
      2500.0, 2500.0, 625.0, 625.0, 10000.0, 250000.0};
    std::array<std::size_t, 2U * kStateDim> global{};
    for (std::size_t index = 0U; index < kStateDim; ++index) {
      global[index] = left_index * kStateDim + index;
      global[kStateDim + index] = right_index * kStateDim + index;
    }
    for (std::size_t a = 0U; a < global.size(); ++a) {
      for (std::size_t residual_index = 0U; residual_index < kStateDim; ++residual_index) {
        gradient_[global[a]] += jacobian[residual_index][a] *
          weights[residual_index] * residual[residual_index];
      }
      for (std::size_t b = 0U; b < global.size(); ++b) {
        for (std::size_t residual_index = 0U; residual_index < kStateDim; ++residual_index) {
          hessian_[global[a]][global[b]] += jacobian[residual_index][a] *
            weights[residual_index] * jacobian[residual_index][b];
        }
      }
    }
  }

  void add_visual_factor(std::size_t state_index)
  {
    const State & state = states_[state_index];
    const std::size_t base = state_index * kStateDim;
    const std::array<std::size_t, 3U> local_indices{0U, 1U, 4U};
    const std::array<double, 3U> residuals{
      state.x - state.visual.x,
      state.y - state.visual.y,
      wrap_angle(state.yaw - state.visual.yaw)};
    const std::array<double, 3U> weights{
      1.0 / state.visual.var_x,
      1.0 / state.visual.var_y,
      1.0 / state.visual.var_yaw};
    for (std::size_t index = 0U; index < local_indices.size(); ++index) {
      const std::size_t global = base + local_indices[index];
      hessian_[global][global] += weights[index];
      gradient_[global] += weights[index] * residuals[index];
    }
  }

  void add_initial_anchor()
  {
    const State & first = states_[0];
    const std::array<double, kStateDim> target{
      first.visual.x, first.visual.y, 0.0, 0.0, first.visual.yaw, 0.0};
    const std::array<double, kStateDim> weights{
      10000.0, 10000.0, 25.0, 25.0, 40000.0, 100.0};
    for (std::size_t index = 0U; index < kStateDim; ++index) {
      double residual = component(first, index) - target[index];
      if (index == 4U) {
        residual = wrap_angle(residual);
      }
      hessian_[index][index] += weights[index];
      gradient_[index] += weights[index] * residual;
    }
  }

  void add_marginal_prior()
  {
    Vector delta{};
    for (std::size_t index = 0U; index < prior_dimension_; ++index) {
      const std::size_t state_index = index / kStateDim;
      const std::size_t local = index % kStateDim;
      delta[index] = component(states_[state_index], local) - prior_reference_[index];
      if (local == 4U) {
        delta[index] = wrap_angle(delta[index]);
      }
    }
    for (std::size_t row = 0U; row < prior_dimension_; ++row) {
      hessian_[row][row] += 1.0e-9;
      for (std::size_t column = 0U; column < prior_dimension_; ++column) {
        hessian_[row][column] += prior_hessian_[row][column];
        gradient_[row] += prior_hessian_[row][column] * delta[column];
      }
      gradient_[row] += prior_gradient_[row];
    }
  }

  void build_system()
  {
    const std::size_t dimension = state_count_ * kStateDim;
    clear_system(dimension);
    std::size_t first_new_state = 0U;
    if (prior_valid_) {
      add_marginal_prior();
      first_new_state = prior_dimension_ / kStateDim;
    } else {
      add_initial_anchor();
    }

    for (std::size_t index = first_new_state; index < state_count_; ++index) {
      add_visual_factor(index);
      if (index > 0U) {
        add_imu_factor(index - 1U, index);
      }
    }
    // Levenberg 형태의 작은 diagonal은 수치적으로 거의 특이한 축에서 pivot 붕괴를 막는다.
    for (std::size_t index = 0U; index < dimension; ++index) {
      hessian_[index][index] += 1.0e-6;
    }
  }

  bool solve_linear_system(std::size_t dimension, Vector & solution)
  {
    std::array<std::array<double, kMaxDim + 1U>, kMaxDim> augmented{};
    for (std::size_t row = 0U; row < dimension; ++row) {
      for (std::size_t column = 0U; column < dimension; ++column) {
        augmented[row][column] = hessian_[row][column];
      }
      augmented[row][dimension] = -gradient_[row];
    }

    for (std::size_t pivot_column = 0U; pivot_column < dimension; ++pivot_column) {
      std::size_t pivot_row = pivot_column;
      double pivot_value = std::abs(augmented[pivot_row][pivot_column]);
      for (std::size_t row = pivot_column + 1U; row < dimension; ++row) {
        const double candidate = std::abs(augmented[row][pivot_column]);
        if (candidate > pivot_value) {
          pivot_value = candidate;
          pivot_row = row;
        }
      }
      if (!(pivot_value > 1.0e-10) || !std::isfinite(pivot_value)) {
        return false;
      }
      if (pivot_row != pivot_column) {
        std::swap(augmented[pivot_row], augmented[pivot_column]);
      }
      for (std::size_t row = pivot_column + 1U; row < dimension; ++row) {
        const double ratio = augmented[row][pivot_column] /
          augmented[pivot_column][pivot_column];
        for (std::size_t column = pivot_column; column <= dimension; ++column) {
          augmented[row][column] -= ratio * augmented[pivot_column][column];
        }
      }
    }

    for (std::size_t reverse = dimension; reverse > 0U; --reverse) {
      const std::size_t row = reverse - 1U;
      double value = augmented[row][dimension];
      for (std::size_t column = row + 1U; column < dimension; ++column) {
        value -= augmented[row][column] * solution[column];
      }
      solution[row] = value / augmented[row][row];
      if (!std::isfinite(solution[row])) {
        return false;
      }
    }
    return true;
  }

  bool solve_window()
  {
    const std::size_t dimension = state_count_ * kStateDim;
    for (std::size_t iteration = 0U; iteration < kSolverIterations; ++iteration) {
      build_system();
      Vector increment{};
      if (!solve_linear_system(dimension, increment)) {
        return false;
      }
      for (std::size_t state_index = 0U; state_index < state_count_; ++state_index) {
        State & state = states_[state_index];
        state.x += clamp_step(increment[state_index * kStateDim], 0.25);
        state.y += clamp_step(increment[state_index * kStateDim + 1U], 0.25);
        state.vx += clamp_step(increment[state_index * kStateDim + 2U], 0.25);
        state.vy += clamp_step(increment[state_index * kStateDim + 3U], 0.25);
        state.yaw = wrap_angle(
          state.yaw + clamp_step(increment[state_index * kStateDim + 4U], 0.15));
        state.gyro_bias += clamp_step(increment[state_index * kStateDim + 5U], 0.02);
      }
    }
    return true;
  }

  bool invert_oldest_block(
    std::array<std::array<double, kStateDim>, kStateDim> & inverse,
    double & minimum_pivot)
  {
    std::array<std::array<double, 2U * kStateDim>, kStateDim> augmented{};
    for (std::size_t row = 0U; row < kStateDim; ++row) {
      for (std::size_t column = 0U; column < kStateDim; ++column) {
        augmented[row][column] = hessian_[row][column];
      }
      augmented[row][kStateDim + row] = 1.0;
    }
    minimum_pivot = std::numeric_limits<double>::infinity();
    for (std::size_t column = 0U; column < kStateDim; ++column) {
      std::size_t pivot_row = column;
      double pivot = std::abs(augmented[pivot_row][column]);
      for (std::size_t row = column + 1U; row < kStateDim; ++row) {
        const double candidate = std::abs(augmented[row][column]);
        if (candidate > pivot) {
          pivot = candidate;
          pivot_row = row;
        }
      }
      if (!(pivot > 1.0e-10) || !std::isfinite(pivot)) {
        return false;
      }
      minimum_pivot = std::min(minimum_pivot, pivot);
      if (pivot_row != column) {
        std::swap(augmented[pivot_row], augmented[column]);
      }
      const double diagonal = augmented[column][column];
      for (double & value : augmented[column]) {
        value /= diagonal;
      }
      for (std::size_t row = 0U; row < kStateDim; ++row) {
        if (row == column) {
          continue;
        }
        const double ratio = augmented[row][column];
        for (std::size_t entry = 0U; entry < 2U * kStateDim; ++entry) {
          augmented[row][entry] -= ratio * augmented[column][entry];
        }
      }
    }
    for (std::size_t row = 0U; row < kStateDim; ++row) {
      for (std::size_t column = 0U; column < kStateDim; ++column) {
        inverse[row][column] = augmented[row][kStateDim + column];
      }
    }
    return true;
  }

  bool marginalize_oldest()
  {
    // 최종 선형화점에서 H,b를 다시 만들고 x_old를 Schur complement로 제거한다.
    // H_m = H_rr - H_ro H_oo^{-1} H_or, b_m = b_r - H_ro H_oo^{-1} b_o.
    build_system();
    std::array<std::array<double, kStateDim>, kStateDim> oldest_inverse{};
    double pivot = 0.0;
    if (!invert_oldest_block(oldest_inverse, pivot)) {
      return false;
    }
    minimum_marginal_pivot_ = std::min(minimum_marginal_pivot_, pivot);

    const std::size_t remaining_dimension = (state_count_ - 1U) * kStateDim;
    std::array<std::array<double, kStateDim>, kPriorDim> h_ro_times_inverse{};
    for (std::size_t row = 0U; row < remaining_dimension; ++row) {
      for (std::size_t column = 0U; column < kStateDim; ++column) {
        for (std::size_t inner = 0U; inner < kStateDim; ++inner) {
          h_ro_times_inverse[row][column] +=
            hessian_[kStateDim + row][inner] * oldest_inverse[inner][column];
        }
      }
    }

    for (std::size_t row = 0U; row < remaining_dimension; ++row) {
      double reduced_gradient = gradient_[kStateDim + row];
      for (std::size_t inner = 0U; inner < kStateDim; ++inner) {
        reduced_gradient -= h_ro_times_inverse[row][inner] * gradient_[inner];
      }
      prior_gradient_[row] = reduced_gradient;
      for (std::size_t column = 0U; column < remaining_dimension; ++column) {
        double reduced_hessian = hessian_[kStateDim + row][kStateDim + column];
        for (std::size_t inner = 0U; inner < kStateDim; ++inner) {
          reduced_hessian -= h_ro_times_inverse[row][inner] *
            hessian_[inner][kStateDim + column];
        }
        prior_hessian_[row][column] = reduced_hessian;
      }
    }

    for (std::size_t index = 1U; index < state_count_; ++index) {
      states_[index - 1U] = states_[index];
    }
    --state_count_;
    prior_dimension_ = remaining_dimension;
    for (std::size_t index = 0U; index < prior_dimension_; ++index) {
      const std::size_t state_index = index / kStateDim;
      const std::size_t local = index % kStateDim;
      prior_reference_[index] = component(states_[state_index], local);
    }
    prior_valid_ = true;
    return true;
  }

  void publish_result(
    const VisualSample & visual, std::uint32_t interval_samples,
    double interval_seconds, double elapsed_us)
  {
    const State & latest = states_[state_count_ - 1U];
    nav_msgs::msg::Odometry odometry;
    // Jazzy에서 정수 nanosecond를 builtin_interfaces/Time으로 명시적으로 나눈다.
    odometry.header.stamp.sec = static_cast<std::int32_t>(visual.stamp_ns / 1000000000LL);
    odometry.header.stamp.nanosec = static_cast<std::uint32_t>(visual.stamp_ns % 1000000000LL);
    odometry.header.frame_id = "map";
    odometry.child_frame_id = "base_link";
    odometry.pose.pose.position.x = latest.x;
    odometry.pose.pose.position.y = latest.y;
    set_yaw(latest.yaw, odometry.pose.pose.orientation);
    odometry.pose.covariance[0] = 4.0e-4;
    odometry.pose.covariance[7] = 4.0e-4;
    odometry.pose.covariance[35] = 1.0e-4;

    // Odometry.twist는 child_frame_id 기준이므로 world velocity를 R^T로 base_link에 회전한다.
    const double c = std::cos(latest.yaw);
    const double s = std::sin(latest.yaw);
    odometry.twist.twist.linear.x = c * latest.vx + s * latest.vy;
    odometry.twist.twist.linear.y = -s * latest.vx + c * latest.vy;
    odometry_publisher_->publish(odometry);

    daily_robotics_2026_10_24::msg::PreintegrationStats stats;
    stats.header = odometry.header;
    stats.sequence = ++sequence_;
    stats.window_states = static_cast<std::uint32_t>(state_count_);
    stats.imu_samples_total = imu_samples_total_;
    stats.latest_preintegrated_samples = interval_samples;
    stats.imu_queue_drops = imu_queue_drops_.load(std::memory_order_relaxed);
    stats.visual_queue_drops = visual_queue_drops_.load(std::memory_order_relaxed);
    stats.staged_imu_drops = staged_imu_drops_;
    stats.stale_visual_drops = stale_visual_drops_;
    stats.marginalizations = marginalizations_;
    stats.solve_failures = solve_failures_;
    stats.max_imu_batch = max_imu_batch_;
    stats.latest_interval_sec = interval_seconds;
    stats.estimated_gyro_bias = latest.gyro_bias;
    stats.latest_kernel_us = elapsed_us;
    stats.max_kernel_us = max_kernel_us_;
    stats.minimum_marginal_pivot = std::isfinite(minimum_marginal_pivot_) ?
      minimum_marginal_pivot_ : 0.0;
    stats.bounds_respected = state_count_ <= kMaxStates &&
      staged_imu_count_ <= kStagedImuCapacity && max_imu_batch_ <= kMaxImuDrainPerCycle;
    stats_publisher_->publish(stats);
  }

  rclcpp::CallbackGroup::SharedPtr imu_group_;
  rclcpp::CallbackGroup::SharedPtr visual_group_;
  rclcpp::CallbackGroup::SharedPtr worker_group_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr
    visual_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odometry_publisher_;
  rclcpp::Publisher<daily_robotics_2026_10_24::msg::PreintegrationStats>::SharedPtr
    stats_publisher_;
  rclcpp::TimerBase::SharedPtr worker_timer_;

  SpscRing<ImuSample, kImuQueueSize> imu_queue_;
  SpscRing<VisualSample, kVisualQueueSize> visual_queue_;
  std::array<ImuSample, kStagedImuCapacity> staged_imus_{};
  std::size_t staged_imu_count_{0U};
  VisualSample pending_visual_{};
  bool has_pending_visual_{false};

  std::array<State, kMaxStates> states_{};
  std::size_t state_count_{0U};
  bool initialized_{false};
  std::int64_t last_keyframe_stamp_ns_{0};
  Preintegrator preintegrator_{};

  Matrix hessian_{};
  Vector gradient_{};
  std::array<std::array<double, kPriorDim>, kPriorDim> prior_hessian_{};
  std::array<double, kPriorDim> prior_gradient_{};
  std::array<double, kPriorDim> prior_reference_{};
  std::size_t prior_dimension_{0U};
  bool prior_valid_{false};

  alignas(64) std::atomic<std::uint32_t> imu_queue_drops_{0U};
  alignas(64) std::atomic<std::uint32_t> visual_queue_drops_{0U};
  std::uint32_t staged_imu_drops_{0U};
  std::uint32_t stale_visual_drops_{0U};
  std::uint32_t imu_samples_total_{0U};
  std::uint32_t marginalizations_{0U};
  std::uint32_t solve_failures_{0U};
  std::uint32_t max_imu_batch_{0U};
  std::uint32_t sequence_{0U};
  double max_kernel_us_{0.0};
  double minimum_marginal_pivot_{std::numeric_limits<double>::infinity()};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<BoundedPreintegrationSmoother>();
  // MultiThreadedExecutor는 producer callback과 numerical worker를 병행한다.
  // 실제 RT 우선순위/CPU affinity는 executor만으로 생기지 않으며 OS 설정과 측정이 별도로 필요하다.
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 3U);
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
