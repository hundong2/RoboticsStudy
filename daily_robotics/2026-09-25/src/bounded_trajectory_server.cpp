#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>
#include <thread>

// mlockall은 제어 중 page fault 가능성을 줄이기 위해 메모리 페이지를 RAM에 고정하는 Linux API다.
#include <sys/mman.h>
// pthread_setschedparam은 제어 스레드에 SCHED_FIFO 우선순위를 요청한다.
#include <pthread.h>
#include <time.h>

#include "builtin_interfaces/msg/duration.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

// rosidl_generate_interfaces가 오늘의 .action/.msg 파일에서 만든 타입이다.
#include "daily_robotics_2026_09_25/action/execute_joint_trajectory.hpp"
#include "daily_robotics_2026_09_25/msg/tracking_frame.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr std::size_t kJointCount = 3;
constexpr double kNanosecondsPerSecond = 1'000'000'000.0;

/**
 * 한 관절의 정규화 시간 s=t/T 다항식 q(s)=c0+c1*s+...+c5*s^5 계수다.
 *
 * std::array는 크기가 컴파일 시점에 고정되어 hot path에서 resize나 heap 할당을 하지 않는다.
 */
struct QuinticAxis
{
  std::array<double, 6> coefficient{};
};

/** Goal을 비 RT Action callback에서 검증·변환한 뒤 제어 스레드로 넘기는 고정 크기 계획이다. */
struct FixedPlan
{
  std::array<QuinticAxis, kJointCount> axis{};
  std::array<double, kJointCount> start_position{};
  std::array<double, kJointCount> start_velocity{};
  double duration_s{1.0};
  double tolerance_rad{0.02};
};

/** Goal의 ROS Duration을 부동소수점 초로 바꾼다. nanosec는 항상 1e9 미만이어야 한다. */
double duration_seconds(const builtin_interfaces::msg::Duration & duration)
{
  return static_cast<double>(duration.sec) +
         static_cast<double>(duration.nanosec) / kNanosecondsPerSecond;
}

/** Feedback용 초 값을 sec/nanosec ROS Duration으로 바꾸며 반올림 overflow를 정규화한다. */
builtin_interfaces::msg::Duration seconds_to_duration(const double seconds)
{
  builtin_interfaces::msg::Duration duration;
  const double nonnegative = std::max(0.0, seconds);
  duration.sec = static_cast<std::int32_t>(std::floor(nonnegative));
  duration.nanosec = static_cast<std::uint32_t>(
    std::llround((nonnegative - static_cast<double>(duration.sec)) * kNanosecondsPerSecond));
  if (duration.nanosec >= 1'000'000'000U) {
    ++duration.sec;
    duration.nanosec -= 1'000'000'000U;
  }
  return duration;
}

/** CLOCK_MONOTONIC 값을 ns 정수로 읽는다. 시스템 시각 보정에 제어 경과 시간이 흔들리지 않는다. */
std::int64_t monotonic_now_ns()
{
  timespec now{};
  ::clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<std::int64_t>(now.tv_sec) * 1'000'000'000LL + now.tv_nsec;
}

/** timespec에 ns를 더해 상대 sleep 누적 오차가 없는 다음 절대 deadline을 만든다. */
void add_nanoseconds(timespec & time, const std::int64_t nanoseconds)
{
  time.tv_nsec += static_cast<long>(nanoseconds);
  while (time.tv_nsec >= 1'000'000'000L) {
    time.tv_nsec -= 1'000'000'000L;
    ++time.tv_sec;
  }
}
}  // namespace

/**
 * 3축 ExecuteJointTrajectory Action과 500 Hz 고정 크기 수치 커널을 연결하는 서버다.
 *
 * Goal/Feedback/Result, 로그, DDS publish는 일반 ROS executor/timer에서 처리한다. 별도 제어
 * 스레드는 std::array 수치 연산과 atomic load/store만 수행한다. 따라서 "수치 커널의 경계"는
 * 분명하지만, 일반 Linux와 DDS 전체를 hard real-time이라고 주장하지는 않는다.
 */
class BoundedTrajectoryServer : public rclcpp::Node
{
public:
  using ExecuteJointTrajectory =
    daily_robotics_2026_09_25::action::ExecuteJointTrajectory;
  using GoalHandleTrajectory =
    rclcpp_action::ServerGoalHandle<ExecuteJointTrajectory>;
  using TrackingFrame = daily_robotics_2026_09_25::msg::TrackingFrame;

  BoundedTrajectoryServer()
  : Node("bounded_trajectory_server")
  {
    // 파라미터는 장비/커널별 주기와 우선순위를 재빌드 없이 바꾸게 해준다.
    control_frequency_hz_ = declare_parameter<double>("control_frequency_hz", 500.0);
    rt_priority_ = declare_parameter<int>("rt_priority", 60);

    if (control_frequency_hz_ < 50.0 || control_frequency_hz_ > 2000.0) {
      throw std::invalid_argument("control_frequency_hz must be in [50, 2000]");
    }

    // Transient Local은 auditor가 terminal frame 직후 늦게 구독해도 마지막 결과를 받게 한다.
    tracking_publisher_ = create_publisher<TrackingFrame>(
      "/trajectory/tracking_frame", rclcpp::QoS(1).reliable().transient_local());

    // create_server는 하나의 Action 이름에 Goal 검증, Cancel, Accepted callback을 등록한다.
    action_server_ = rclcpp_action::create_server<ExecuteJointTrajectory>(
      this,
      "/execute_joint_trajectory",
      std::bind(
        &BoundedTrajectoryServer::handle_goal, this,
        std::placeholders::_1, std::placeholders::_2),
      std::bind(
        &BoundedTrajectoryServer::handle_cancel, this,
        std::placeholders::_1),
      std::bind(
        &BoundedTrajectoryServer::handle_accepted, this,
        std::placeholders::_1));

    // 20 ms(50 Hz) 타이머가 atomic snapshot을 ROS 메시지로 직렬화한다. 500 Hz 계산과 분리된다.
    feedback_timer_ = create_wall_timer(
      20ms, std::bind(&BoundedTrajectoryServer::publish_feedback_or_result, this));

    // 제어 스레드는 노드 초기화 때 한 번만 만든다. Goal마다 생성하면 생성 지연이 달라진다.
    control_thread_ = std::thread(&BoundedTrajectoryServer::control_loop, this);

    bool all_lock_free = control_active_.is_lock_free() && elapsed_s_.is_lock_free() &&
      telemetry_sequence_.is_lock_free();
    for (std::size_t joint = 0; joint < kJointCount; ++joint) {
      all_lock_free = all_lock_free && desired_position_[joint].is_lock_free() &&
        actual_position_[joint].is_lock_free();
    }
    RCLCPP_INFO(
      get_logger(), "500 Hz kernel ready; scalar atomics lock_free=%s",
      all_lock_free ? "true" : "false");
  }

  ~BoundedTrajectoryServer() override
  {
    // this가 파괴된 뒤 스레드가 멤버를 읽지 않도록 종료 플래그 뒤 join 순서를 지킨다.
    running_.store(false, std::memory_order_release);
    if (control_thread_.joinable()) {
      control_thread_.join();
    }
  }

private:
  /** 6개 경계조건 q(0),v(0),a(0),q(T),v(T),a(T)를 만족하는 계수를 닫힌형으로 계산한다. */
  static QuinticAxis make_quintic(
    const double q0, const double v0, const double acceleration0,
    const double q1, const double v1, const double acceleration1,
    const double duration_s)
  {
    QuinticAxis axis;
    auto & c = axis.coefficient;

    // s=t/T를 쓰므로 dq/ds=T*dq/dt, d2q/ds2=T^2*d2q/dt2가 된다.
    c[0] = q0;
    c[1] = duration_s * v0;
    c[2] = 0.5 * duration_s * duration_s * acceleration0;

    // A/B/C는 종료 경계에서 c0,c1,c2가 이미 기여한 양을 뺀 잔차다.
    const double a = q1 - (c[0] + c[1] + c[2]);
    const double b = duration_s * v1 - (c[1] + 2.0 * c[2]);
    const double d = duration_s * duration_s * acceleration1 - 2.0 * c[2];

    // [c3,c4,c5]에 대한 3x3 선형식을 미리 풀어 둔 닫힌형이다.
    c[3] = 10.0 * a - 4.0 * b + 0.5 * d;
    c[4] = -15.0 * a + 7.0 * b - d;
    c[5] = 6.0 * a - 3.0 * b + 0.5 * d;
    return axis;
  }

  /** Horner 형태로 q,v,a를 샘플링해 곱셈 횟수와 수치 오차를 줄인다. */
  static void sample_quintic(
    const QuinticAxis & axis, const double normalized_time, const double duration_s,
    double & position, double & velocity, double & acceleration)
  {
    const auto & c = axis.coefficient;
    const double s = std::clamp(normalized_time, 0.0, 1.0);

    // q(s)=c0+s(c1+s(c2+s(c3+s(c4+s*c5))))
    position = c[0] + s * (c[1] + s * (c[2] + s * (c[3] + s * (c[4] + s * c[5]))));
    // v(t)=(dq/ds)/T, a(t)=(d2q/ds2)/T^2: 정규화 시간 미분을 실제 시간 미분으로 복원한다.
    velocity =
      (c[1] + s * (2.0 * c[2] + s * (3.0 * c[3] + s * (4.0 * c[4] + s * 5.0 * c[5])))) /
      duration_s;
    acceleration =
      (2.0 * c[2] + s * (6.0 * c[3] + s * (12.0 * c[4] + s * 20.0 * c[5]))) /
      (duration_s * duration_s);
  }

  /** 배열의 모든 수가 유한하고 실습 범위 안인지 확인해 NaN/Inf가 제어식에 들어오는 것을 막는다. */
  static bool boundary_is_valid(
    const std::array<double, kJointCount> & position,
    const std::array<double, kJointCount> & velocity,
    const std::array<double, kJointCount> & acceleration)
  {
    for (std::size_t joint = 0; joint < kJointCount; ++joint) {
      if (!std::isfinite(position[joint]) || !std::isfinite(velocity[joint]) ||
        !std::isfinite(acceleration[joint]) || std::abs(position[joint]) > 6.3 ||
        std::abs(velocity[joint]) > 4.0 || std::abs(acceleration[joint]) > 20.0)
      {
        return false;
      }
    }
    return true;
  }

  /** 새 Goal의 수치 범위와 "동시에 하나" 정책을 장치 상태 변경 전에 검사한다. */
  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID &,
    std::shared_ptr<const ExecuteJointTrajectory::Goal> goal)
  {
    const double duration_s = duration_seconds(goal->duration);
    const bool valid =
      boundary_is_valid(
        goal->start_position_rad, goal->start_velocity_rad_s,
        goal->start_acceleration_rad_s2) &&
      boundary_is_valid(
        goal->goal_position_rad, goal->goal_velocity_rad_s,
        goal->goal_acceleration_rad_s2) &&
      std::isfinite(duration_s) && duration_s >= 0.5 && duration_s <= 10.0 &&
      std::isfinite(goal->position_tolerance_rad) &&
      goal->position_tolerance_rad >= 0.001 && goal->position_tolerance_rad <= 0.2;

    if (!valid) {
      RCLCPP_WARN(
        get_logger(),
        "Goal rejected: finite bounded states, duration [0.5,10] s, tolerance [0.001,0.2] rad required");
      return rclcpp_action::GoalResponse::REJECT;
    }

    // compare_exchange는 거의 동시에 온 두 Goal 중 정확히 하나만 실행 자리를 예약하게 한다.
    bool expected = false;
    if (!goal_reserved_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      RCLCPP_WARN(get_logger(), "Goal rejected: another trajectory is active");
      return rclcpp_action::GoalResponse::REJECT;
    }
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  /** Cancel 수락을 RT 루프가 기다림 없이 읽는 atomic 플래그로 넘긴다. */
  rclcpp_action::CancelResponse handle_cancel(
    const std::shared_ptr<GoalHandleTrajectory> goal_handle)
  {
    std::lock_guard<std::mutex> lock(goal_mutex_);
    if (!active_goal_ || active_goal_ != goal_handle) {
      return rclcpp_action::CancelResponse::REJECT;
    }
    cancel_requested_.store(true, std::memory_order_release);
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  /** 수락된 동적 ROS Goal을 고정 크기 계수/상태로 바꾼 뒤 release-store로 제어 스레드에 공개한다. */
  void handle_accepted(const std::shared_ptr<GoalHandleTrajectory> goal_handle)
  {
    const auto goal = goal_handle->get_goal();
    const double duration_s = duration_seconds(goal->duration);

    FixedPlan next_plan;
    next_plan.start_position = goal->start_position_rad;
    next_plan.start_velocity = goal->start_velocity_rad_s;
    next_plan.duration_s = duration_s;
    next_plan.tolerance_rad = goal->position_tolerance_rad;
    for (std::size_t joint = 0; joint < kJointCount; ++joint) {
      next_plan.axis[joint] = make_quintic(
        goal->start_position_rad[joint], goal->start_velocity_rad_s[joint],
        goal->start_acceleration_rad_s2[joint], goal->goal_position_rad[joint],
        goal->goal_velocity_rad_s[joint], goal->goal_acceleration_rad_s2[joint],
        duration_s);
    }

    {
      // GoalHandle은 Cancel/feedback callback과 공유되므로 비 RT 영역에서만 짧은 mutex로 보호한다.
      std::lock_guard<std::mutex> lock(goal_mutex_);
      active_goal_ = goal_handle;
    }

    // goal_reserved=true인 동안 writer는 하나뿐이고 RT 루프는 inactive라 plain struct copy가 안전하다.
    plan_ = next_plan;
    // 홀수 sequence는 writer가 여러 atomic field를 갱신 중임을 뜻한다. reader는 이 구간을 건너뛴다.
    telemetry_sequence_.fetch_add(1, std::memory_order_acq_rel);
    // 이전 Goal의 snapshot이 새 Goal의 첫 Feedback으로 보이지 않게 공개 전 telemetry를 초기화한다.
    for (std::size_t joint = 0; joint < kJointCount; ++joint) {
      desired_position_[joint].store(goal->start_position_rad[joint], std::memory_order_relaxed);
      desired_velocity_[joint].store(goal->start_velocity_rad_s[joint], std::memory_order_relaxed);
      desired_acceleration_[joint].store(
        goal->start_acceleration_rad_s2[joint], std::memory_order_relaxed);
      actual_position_[joint].store(goal->start_position_rad[joint], std::memory_order_relaxed);
      actual_velocity_[joint].store(goal->start_velocity_rad_s[joint], std::memory_order_relaxed);
    }
    elapsed_s_.store(0.0, std::memory_order_relaxed);
    current_error_rad_.store(0.0, std::memory_order_relaxed);
    max_error_rad_.store(0.0, std::memory_order_relaxed);
    control_ticks_.store(0, std::memory_order_relaxed);
    max_wakeup_lateness_ns_.store(0, std::memory_order_relaxed);
    // 짝수 sequence를 release하면 위 fixed snapshot 전체가 reader에게 한 프레임으로 보인다.
    telemetry_sequence_.fetch_add(1, std::memory_order_release);
    cancel_requested_.store(false, std::memory_order_relaxed);
    cancel_acknowledged_.store(false, std::memory_order_relaxed);
    goal_done_.store(false, std::memory_order_relaxed);
    goal_failed_.store(false, std::memory_order_relaxed);
    plan_generation_.fetch_add(1, std::memory_order_release);
    // acquire로 active=true를 본 제어 스레드는 위 계획/플래그 쓰기까지 함께 관찰한다.
    control_active_.store(true, std::memory_order_release);

    RCLCPP_INFO(
      get_logger(), "Accepted 3-axis quintic goal: duration=%.3f s tolerance=%.4f rad",
      duration_s, goal->position_tolerance_rad);
  }

  /** atomic telemetry를 50 Hz ROS 메시지와 Action Feedback으로 직렬화하고 종료 상태를 확정한다. */
  void publish_feedback_or_result()
  {
    std::shared_ptr<GoalHandleTrajectory> goal;
    {
      std::lock_guard<std::mutex> lock(goal_mutex_);
      goal = active_goal_;
    }
    if (!goal) {
      return;
    }

    const bool canceled = cancel_acknowledged_.load(std::memory_order_acquire);
    const bool done = goal_done_.load(std::memory_order_acquire);
    const bool failed = goal_failed_.load(std::memory_order_acquire);
    const bool terminal = canceled || done || failed;

    TrackingFrame frame;
    double snapshot_elapsed_s = 0.0;
    bool snapshot_consistent = false;
    // 모든 payload field 자체도 atomic이므로 C++ data race가 없다. sequence는 서로 다른 tick의
    // desired/actual을 한 ROS frame에 섞지 않기 위한 일관성 검사다. timer callback은 최대 5번만 재시도한다.
    for (int attempt = 0; attempt < 5; ++attempt) {
      const std::uint64_t before = telemetry_sequence_.load(std::memory_order_acquire);
      if ((before & 1U) != 0U) {
        continue;
      }
      for (std::size_t joint = 0; joint < kJointCount; ++joint) {
        frame.desired_position_rad[joint] = desired_position_[joint].load(std::memory_order_relaxed);
        frame.desired_velocity_rad_s[joint] = desired_velocity_[joint].load(std::memory_order_relaxed);
        frame.desired_acceleration_rad_s2[joint] =
          desired_acceleration_[joint].load(std::memory_order_relaxed);
        frame.actual_position_rad[joint] = actual_position_[joint].load(std::memory_order_relaxed);
        frame.actual_velocity_rad_s[joint] = actual_velocity_[joint].load(std::memory_order_relaxed);
      }
      snapshot_elapsed_s = elapsed_s_.load(std::memory_order_relaxed);
      frame.max_position_error_rad = current_error_rad_.load(std::memory_order_relaxed);
      frame.wakeup_lateness_ns = max_wakeup_lateness_ns_.load(std::memory_order_relaxed);
      frame.control_ticks = control_ticks_.load(std::memory_order_relaxed);
      const std::uint64_t after = telemetry_sequence_.load(std::memory_order_acquire);
      if (before == after && (after & 1U) == 0U) {
        snapshot_consistent = true;
        break;
      }
    }
    if (!snapshot_consistent) {
      return;
    }

    // Accepted callback과 첫 500 Hz tick 사이의 tick=0 snapshot은 실제 궤적 표본이 아니므로 건너뛴다.
    if (frame.control_ticks == 0U && !terminal) {
      return;
    }

    // Header.stamp는 DDS 발행 시각이며, 제어 경과 시간은 Action Feedback elapsed로 따로 보낸다.
    frame.header.stamp = now();
    frame.header.frame_id = "joint_space";
    frame.terminal = terminal;
    tracking_publisher_->publish(frame);

    if (!terminal) {
      auto feedback = std::make_shared<ExecuteJointTrajectory::Feedback>();
      feedback->elapsed = seconds_to_duration(snapshot_elapsed_s);
      feedback->desired_position_rad = frame.desired_position_rad;
      feedback->actual_position_rad = frame.actual_position_rad;
      feedback->current_position_error_rad = frame.max_position_error_rad;
      // Feedback는 UI/감시용 50 Hz이고 500 Hz 제어 deadline 안에서 publish하지 않는다.
      goal->publish_feedback(feedback);
      return;
    }

    auto result = std::make_shared<ExecuteJointTrajectory::Result>();
    result->max_position_error_rad = max_error_rad_.load(std::memory_order_relaxed);
    result->control_ticks = control_ticks_.load(std::memory_order_relaxed);

    if (canceled) {
      result->success = false;
      result->message = "Cancel acknowledged at the bounded-kernel boundary";
      goal->canceled(result);
    } else if (failed) {
      result->success = false;
      result->message = "Terminal tracking error exceeded the requested tolerance";
      goal->abort(result);
    } else {
      result->success = true;
      result->message = "All three axes reached the nonzero terminal boundary within tolerance";
      goal->succeed(result);
    }
    reset_goal_state();
  }

  /** 종료 뒤 공유 상태를 다음 Goal이 정확히 한 번 예약할 수 있는 상태로 되돌린다. */
  void reset_goal_state()
  {
    control_active_.store(false, std::memory_order_release);
    {
      std::lock_guard<std::mutex> lock(goal_mutex_);
      active_goal_.reset();
    }
    cancel_requested_.store(false, std::memory_order_relaxed);
    cancel_acknowledged_.store(false, std::memory_order_relaxed);
    goal_done_.store(false, std::memory_order_relaxed);
    goal_failed_.store(false, std::memory_order_relaxed);
    goal_reserved_.store(false, std::memory_order_release);
  }

  /** 메모리 잠금과 SCHED_FIFO를 시도한다. 권한이 없으면 경고하고 일반 스케줄러로 계속한다. */
  void configure_realtime_thread()
  {
    if (::mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
      RCLCPP_WARN(
        get_logger(), "mlockall failed (%s): continuing without locked memory",
        std::strerror(errno));
    }

    sched_param scheduling{};
    scheduling.sched_priority = rt_priority_;
    const int result = ::pthread_setschedparam(pthread_self(), SCHED_FIFO, &scheduling);
    if (result != 0) {
      RCLCPP_WARN(
        get_logger(), "SCHED_FIFO priority %d failed (%s): using normal scheduler",
        rt_priority_, std::strerror(result));
    } else {
      RCLCPP_INFO(get_logger(), "SCHED_FIFO priority %d enabled", rt_priority_);
    }

    // 64 KiB 스택을 페이지 간격으로 미리 써서 첫 제어 tick의 demand paging 가능성을 낮춘다.
    volatile char stack_prefault[64 * 1024];
    for (std::size_t index = 0; index < sizeof(stack_prefault); index += 4096) {
      stack_prefault[index] = 0;
    }
  }

  /**
   * 500 Hz에서 3축 5차 궤적을 샘플링하고 feed-forward+PD 모의 관절을 적분한다.
   *
   * 이 함수는 실행 중 ROS publish/log, mutex, vector/string, new/delete를 호출하지 않는다.
   * 실제 로봇에서는 이 위치에 하드웨어 read→control→write와 드라이브 watchdog이 들어간다.
   */
  void control_loop()
  {
    configure_realtime_thread();

    const auto period_ns = static_cast<std::int64_t>(
      std::llround(kNanosecondsPerSecond / control_frequency_hz_));
    const double nominal_dt_s = static_cast<double>(period_ns) / kNanosecondsPerSecond;
    timespec next_wakeup{};
    ::clock_gettime(CLOCK_MONOTONIC, &next_wakeup);

    std::uint64_t observed_generation = 0;
    FixedPlan local_plan;
    std::array<double, kJointCount> actual_position{};
    std::array<double, kJointCount> actual_velocity{};
    std::int64_t start_ns = 0;
    std::int64_t previous_tick_ns = 0;
    std::int64_t local_max_lateness_ns = 0;
    std::uint64_t local_ticks = 0;
    double local_max_error = 0.0;

    while (running_.load(std::memory_order_acquire) && rclcpp::ok()) {
      add_nanoseconds(next_wakeup, period_ns);
      // TIMER_ABSTIME은 한 tick 지연이 다음 tick의 기준 시각에 누적되는 drift를 막는다.
      while (::clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wakeup, nullptr) == EINTR) {
      }
      const std::int64_t now_ns = monotonic_now_ns();
      const std::int64_t scheduled_ns =
        static_cast<std::int64_t>(next_wakeup.tv_sec) * 1'000'000'000LL + next_wakeup.tv_nsec;

      if (!control_active_.load(std::memory_order_acquire)) {
        continue;
      }

      const std::uint64_t generation = plan_generation_.load(std::memory_order_acquire);
      if (generation != observed_generation) {
        // Accepted callback이 release로 공개한 고정 크기 계획을 Goal당 정확히 한 번 복사한다.
        local_plan = plan_;
        observed_generation = generation;
        actual_position = local_plan.start_position;
        actual_velocity = local_plan.start_velocity;
        start_ns = now_ns;
        previous_tick_ns = now_ns - period_ns;
        local_max_lateness_ns = 0;
        local_ticks = 0;
        local_max_error = 0.0;
      }

      if (cancel_requested_.load(std::memory_order_acquire)) {
        control_active_.store(false, std::memory_order_release);
        cancel_acknowledged_.store(true, std::memory_order_release);
        continue;
      }

      const double elapsed = static_cast<double>(now_ns - start_ns) / kNanosecondsPerSecond;
      const double normalized_time = elapsed / local_plan.duration_s;
      // 실제 sleep 간격을 쓰되 0.5~5 tick으로 제한해 디버그 정지 뒤 큰 적분 jump를 막는다.
      const double measured_dt = static_cast<double>(now_ns - previous_tick_ns) / kNanosecondsPerSecond;
      const double dt_s = std::clamp(measured_dt, 0.5 * nominal_dt_s, 5.0 * nominal_dt_s);
      previous_tick_ns = now_ns;

      // 홀수 sequence로 전환한 뒤 한 tick의 모든 scalar를 쓴다. reader는 중간 상태를 쓰지 않는다.
      telemetry_sequence_.fetch_add(1, std::memory_order_acq_rel);
      double current_error = 0.0;
      for (std::size_t joint = 0; joint < kJointCount; ++joint) {
        double desired_position = 0.0;
        double desired_velocity = 0.0;
        double desired_acceleration = 0.0;
        sample_quintic(
          local_plan.axis[joint], normalized_time, local_plan.duration_s,
          desired_position, desired_velocity, desired_acceleration);

        // qdd_cmd=qdd_d+Kp(q_d-q)+Kd(v_d-v): 모델이 맞으면 feed-forward가 곡률을 담당한다.
        const double acceleration_command = std::clamp(
          desired_acceleration + 80.0 * (desired_position - actual_position[joint]) +
          18.0 * (desired_velocity - actual_velocity[joint]),
          -40.0, 40.0);
        // 등가속도 한 tick 적분: q+=v*dt+0.5*a*dt^2, v+=a*dt.
        actual_position[joint] += actual_velocity[joint] * dt_s +
          0.5 * acceleration_command * dt_s * dt_s;
        actual_velocity[joint] += acceleration_command * dt_s;

        const double error = std::abs(desired_position - actual_position[joint]);
        current_error = std::max(current_error, error);
        local_max_error = std::max(local_max_error, error);

        // 단일 writer(RT)·단일 reader(timer)의 scalar snapshot이다. relaxed는 값 자체의 원자성만 필요하다.
        desired_position_[joint].store(desired_position, std::memory_order_relaxed);
        desired_velocity_[joint].store(desired_velocity, std::memory_order_relaxed);
        desired_acceleration_[joint].store(desired_acceleration, std::memory_order_relaxed);
        actual_position_[joint].store(actual_position[joint], std::memory_order_relaxed);
        actual_velocity_[joint].store(actual_velocity[joint], std::memory_order_relaxed);
      }

      ++local_ticks;
      local_max_lateness_ns = std::max<std::int64_t>(
        local_max_lateness_ns, std::max<std::int64_t>(0, now_ns - scheduled_ns));
      elapsed_s_.store(elapsed, std::memory_order_relaxed);
      current_error_rad_.store(current_error, std::memory_order_relaxed);
      max_error_rad_.store(local_max_error, std::memory_order_relaxed);
      control_ticks_.store(local_ticks, std::memory_order_relaxed);
      max_wakeup_lateness_ns_.store(local_max_lateness_ns, std::memory_order_relaxed);
      // 짝수 release는 이 tick의 desired/actual/metrics snapshot 완성을 알린다.
      telemetry_sequence_.fetch_add(1, std::memory_order_release);

      if (elapsed >= local_plan.duration_s) {
        // 종료 허용오차는 Action 의미의 성공/실패를 결정한다. 최대 과도오차와는 별도다.
        control_active_.store(false, std::memory_order_release);
        if (current_error <= local_plan.tolerance_rad) {
          goal_done_.store(true, std::memory_order_release);
        } else {
          goal_failed_.store(true, std::memory_order_release);
        }
      }
    }
  }

  double control_frequency_hz_{500.0};
  int rt_priority_{60};

  rclcpp_action::Server<ExecuteJointTrajectory>::SharedPtr action_server_;
  rclcpp::Publisher<TrackingFrame>::SharedPtr tracking_publisher_;
  rclcpp::TimerBase::SharedPtr feedback_timer_;
  std::thread control_thread_;

  std::atomic<bool> running_{true};
  std::atomic<bool> goal_reserved_{false};
  std::atomic<bool> control_active_{false};
  std::atomic<bool> cancel_requested_{false};
  std::atomic<bool> cancel_acknowledged_{false};
  std::atomic<bool> goal_done_{false};
  std::atomic<bool> goal_failed_{false};
  std::atomic<std::uint64_t> plan_generation_{0};
  std::atomic<std::uint64_t> telemetry_sequence_{0};
  FixedPlan plan_{};

  std::array<std::atomic<double>, kJointCount> desired_position_{};
  std::array<std::atomic<double>, kJointCount> desired_velocity_{};
  std::array<std::atomic<double>, kJointCount> desired_acceleration_{};
  std::array<std::atomic<double>, kJointCount> actual_position_{};
  std::array<std::atomic<double>, kJointCount> actual_velocity_{};
  std::atomic<double> elapsed_s_{0.0};
  std::atomic<double> current_error_rad_{0.0};
  std::atomic<double> max_error_rad_{0.0};
  std::atomic<std::uint64_t> control_ticks_{0};
  std::atomic<std::int64_t> max_wakeup_lateness_ns_{0};

  // GoalHandle은 ROS executor 쪽에서만 mutex로 보호한다. 제어 스레드는 이 객체를 보지 않는다.
  std::mutex goal_mutex_;
  std::shared_ptr<GoalHandleTrajectory> active_goal_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 remapping/parameter CLI를 해석하고 DDS 문맥을 초기화한다.
  rclcpp::init(argc, argv);
  auto node = std::make_shared<BoundedTrajectoryServer>();
  // spin은 Action 요청과 50 Hz feedback timer만 처리한다. 500 Hz 커널은 전용 스레드에 있다.
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
