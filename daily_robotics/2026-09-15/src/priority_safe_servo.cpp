#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <pthread.h>
#include <sched.h>

#include "daily_robotics_2026_09_15/msg/joint_space_world.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/string.hpp"
#include "trajectory_msgs/msg/joint_trajectory.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

namespace
{

constexpr std::size_t kMaxTrajectoryPoints = 1024;
constexpr std::size_t kMaxObstacles = 8;
constexpr double kControlPeriodSeconds = 0.01;

struct TrajectorySnapshot
{
  std::array<double, kMaxTrajectoryPoints> q1{};
  std::array<double, kMaxTrajectoryPoints> q2{};
  std::size_t count{0};
  std::uint64_t version{0};
};

struct WorldSnapshot
{
  std::array<double, kMaxObstacles> q1{};
  std::array<double, kMaxObstacles> q2{};
  std::array<double, kMaxObstacles> radius{};
  std::size_t count{0};
  std::uint64_t version{0};
};

// 원자 필드 기반 seqlock mailbox다. writer가 version을 홀수로 바꾸고 데이터를 쓴 뒤 짝수로 공개한다.
// reader는 앞뒤 version이 같은 짝수일 때만 snapshot을 채택하므로 mutex를 기다리지 않는다.
class AtomicTrajectoryMailbox
{
public:
  void write(const trajectory_msgs::msg::JointTrajectory & message)
  {
    version_.fetch_add(1, std::memory_order_acq_rel);
    const std::size_t count = std::min(message.points.size(), kMaxTrajectoryPoints);
    for (std::size_t i = 0; i < count; ++i) {
      const bool valid = message.points[i].positions.size() >= 2;
      q1_[i].store(valid ? message.points[i].positions[0] : 0.0, std::memory_order_relaxed);
      q2_[i].store(valid ? message.points[i].positions[1] : 0.0, std::memory_order_relaxed);
    }
    count_.store(count, std::memory_order_relaxed);
    version_.fetch_add(1, std::memory_order_release);
  }

  bool read(TrajectorySnapshot & output) const
  {
    // 두 번만 재시도한다. writer와 겹치면 오래 기다리지 않고 제어기가 지난 snapshot을 계속 쓴다.
    for (int attempt = 0; attempt < 2; ++attempt) {
      const std::uint64_t before = version_.load(std::memory_order_acquire);
      if ((before & 1U) != 0U) {
        continue;
      }
      const std::size_t count = count_.load(std::memory_order_relaxed);
      for (std::size_t i = 0; i < count; ++i) {
        output.q1[i] = q1_[i].load(std::memory_order_relaxed);
        output.q2[i] = q2_[i].load(std::memory_order_relaxed);
      }
      const std::uint64_t after = version_.load(std::memory_order_acquire);
      if (before == after && (after & 1U) == 0U) {
        output.count = count;
        output.version = after;
        return true;
      }
    }
    return false;
  }

  bool is_lock_free() const
  {
    return version_.is_lock_free() && count_.is_lock_free() && q1_[0].is_lock_free();
  }

private:
  std::array<std::atomic<double>, kMaxTrajectoryPoints> q1_{};
  std::array<std::atomic<double>, kMaxTrajectoryPoints> q2_{};
  std::atomic<std::size_t> count_{0};
  std::atomic<std::uint64_t> version_{0};
};

class AtomicWorldMailbox
{
public:
  void write(const daily_robotics_2026_09_15::msg::JointSpaceWorld & message)
  {
    version_.fetch_add(1, std::memory_order_acq_rel);
    const std::size_t count = std::min(message.obstacles.size(), kMaxObstacles);
    for (std::size_t i = 0; i < count; ++i) {
      q1_[i].store(message.obstacles[i].x, std::memory_order_relaxed);
      q2_[i].store(message.obstacles[i].y, std::memory_order_relaxed);
      radius_[i].store(message.obstacles[i].z, std::memory_order_relaxed);
    }
    count_.store(count, std::memory_order_relaxed);
    version_.fetch_add(1, std::memory_order_release);
  }

  bool read(WorldSnapshot & output) const
  {
    for (int attempt = 0; attempt < 2; ++attempt) {
      const std::uint64_t before = version_.load(std::memory_order_acquire);
      if ((before & 1U) != 0U) {
        continue;
      }
      const std::size_t count = count_.load(std::memory_order_relaxed);
      for (std::size_t i = 0; i < count; ++i) {
        output.q1[i] = q1_[i].load(std::memory_order_relaxed);
        output.q2[i] = q2_[i].load(std::memory_order_relaxed);
        output.radius[i] = radius_[i].load(std::memory_order_relaxed);
      }
      const std::uint64_t after = version_.load(std::memory_order_acquire);
      if (before == after && (after & 1U) == 0U) {
        output.count = count;
        output.version = after;
        return true;
      }
    }
    return false;
  }

private:
  std::array<std::atomic<double>, kMaxObstacles> q1_{};
  std::array<std::atomic<double>, kMaxObstacles> q2_{};
  std::array<std::atomic<double>, kMaxObstacles> radius_{};
  std::atomic<std::size_t> count_{0};
  std::atomic<std::uint64_t> version_{0};
};

}  // namespace

// 이 노드는 JointTrajectory를 100 Hz 관절 명령으로 추종하면서 장애물 근처에서 속도를 줄인다.
// 고주기 callback과 무거운 진단 callback을 별도 callback group/executor/thread에 배치해 우선순위 역전을 피한다.
class PrioritySafeServoNode final : public rclcpp::Node
{
public:
  PrioritySafeServoNode()
  : Node("priority_safe_servo")
  {
    // 자동 executor 등록을 끄고 main에서 각 group을 서로 다른 executor에 명시적으로 넣는다.
    control_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive, false);
    background_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive, false);

    const auto latched_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    command_pub_ = create_publisher<sensor_msgs::msg::JointState>(
      "/servo/joint_command", rclcpp::SensorDataQoS());
    diagnostics_pub_ = create_publisher<std_msgs::msg::String>(
      "/servo/diagnostics", latched_qos);

    // SubscriptionOptions.callback_group은 입력 역직렬화/복사 callback을 저우선순위 executor로 보낸다.
    rclcpp::SubscriptionOptions input_options;
    input_options.callback_group = background_group_;
    trajectory_sub_ = create_subscription<trajectory_msgs::msg::JointTrajectory>(
      "/planning/joint_trajectory", latched_qos,
      [this](trajectory_msgs::msg::JointTrajectory::SharedPtr message) {
        trajectory_mailbox_.write(*message);
      }, input_options);
    world_sub_ = create_subscription<daily_robotics_2026_09_15::msg::JointSpaceWorld>(
      "/planning/joint_space_world", latched_qos,
      [this](daily_robotics_2026_09_15::msg::JointSpaceWorld::SharedPtr message) {
        world_mailbox_.write(*message);
      }, input_options);

    // 10 ms wall timer가 servo loop다. MutuallyExclusive group은 같은 loop의 중첩 실행을 금지한다.
    control_timer_ = create_wall_timer(
      10ms, std::bind(&PrioritySafeServoNode::control_tick, this), control_group_);
    // 250 ms 진단 callback은 의도적으로 CPU 작업을 포함하지만 별도 executor라 control mutex를 잡지 않는다.
    diagnostics_timer_ = create_wall_timer(
      250ms, std::bind(&PrioritySafeServoNode::background_diagnostics, this), background_group_);

    // hot path에서 vector 크기를 바꾸지 않도록 JointState의 가변 필드를 시작 전에 한 번 할당한다.
    command_message_.name = {"joint1", "joint2"};
    command_message_.position.resize(2);
    command_message_.velocity.resize(2);
  }

  rclcpp::CallbackGroup::SharedPtr control_group() const {return control_group_;}
  rclcpp::CallbackGroup::SharedPtr background_group() const {return background_group_;}
  bool mailbox_lock_free() const {return trajectory_mailbox_.is_lock_free();}

private:
  // 고우선순위 역할: 최신 snapshot을 bounded read하고, 충돌 scale을 적용해 다음 관절 명령을 만든다.
  void control_tick()
  {
    const auto tick_start = std::chrono::steady_clock::now();
    if (have_previous_tick_) {
      const auto period_us = std::chrono::duration_cast<std::chrono::microseconds>(
        tick_start - previous_tick_).count();
      const auto jitter_us = static_cast<std::uint64_t>(std::llabs(period_us - 10000));
      update_max(max_period_jitter_us_, jitter_us);
    }
    previous_tick_ = tick_start;
    have_previous_tick_ = true;

    TrajectorySnapshot incoming;
    if (trajectory_mailbox_.read(incoming)) {
      if (incoming.count > 0 && incoming.version != active_trajectory_.version) {
        active_trajectory_ = incoming;
        active_index_ = std::min<std::size_t>(1, active_trajectory_.count - 1);
        current_q1_ = active_trajectory_.q1[0];
        current_q2_ = active_trajectory_.q2[0];
        reached_.store(false, std::memory_order_relaxed);
      }
    } else {
      snapshot_misses_.fetch_add(1, std::memory_order_relaxed);
    }

    WorldSnapshot incoming_world;
    if (world_mailbox_.read(incoming_world) && incoming_world.version != active_world_.version) {
      active_world_ = incoming_world;
    }

    double velocity_q1 = 0.0;
    double velocity_q2 = 0.0;
    double clearance = minimum_clearance(current_q1_, current_q2_);
    if (active_trajectory_.count > 0) {
      min_clearance_.store(std::min(min_clearance_.load(), clearance), std::memory_order_relaxed);
    }

    if (active_trajectory_.count > 0 && !reached_.load(std::memory_order_relaxed)) {
      double error_q1 = active_trajectory_.q1[active_index_] - current_q1_;
      double error_q2 = active_trajectory_.q2[active_index_] - current_q2_;
      double error_norm = std::hypot(error_q1, error_q2);
      // 중간 waypoint는 정지점이 아니므로 0.08 rad look-ahead 안에 들면 다음 점을 겨냥한다.
      // 마지막 waypoint만 0.025 rad까지 수렴시켜 불필요한 stop-and-go를 막는다.
      if (error_norm < 0.08 && active_index_ + 1 < active_trajectory_.count) {
        ++active_index_;
        error_q1 = active_trajectory_.q1[active_index_] - current_q1_;
        error_q2 = active_trajectory_.q2[active_index_] - current_q2_;
        error_norm = std::hypot(error_q1, error_q2);
      }

      if (active_index_ + 1 == active_trajectory_.count && error_norm < 0.025) {
        reached_.store(true, std::memory_order_relaxed);
      } else if (error_norm > 1e-9) {
        // scale=clamp((clearance-stop)/(slow-stop),0,1): 장애물 표면 0.05 rad 이내면 정지하고
        // 0.20 rad까지 선형 감속한다. 이는 MoveIt Servo의 collision velocity scaling 개념을 축약한 것이다.
        const double collision_scale = std::clamp((clearance - 0.05) / 0.15, 0.0, 1.0);
        // 중간 구간에는 0.8 rad/s 최소 통과 속도를 두되 collision_scale은 언제나 최종 우선권을 갖는다.
        const double speed = std::min(2.0, std::max(0.8, 3.0 * error_norm)) * collision_scale;
        velocity_q1 = speed * error_q1 / error_norm;
        velocity_q2 = speed * error_q2 / error_norm;

        // q[k+1]=q[k]+q_dot*dt: 전진 오일러 적분으로 10 ms 뒤 관절 명령을 계산한다.
        current_q1_ += velocity_q1 * kControlPeriodSeconds;
        current_q2_ += velocity_q2 * kControlPeriodSeconds;
      }
    }

    command_message_.header.stamp = now();
    command_message_.position[0] = current_q1_;
    command_message_.position[1] = current_q2_;
    command_message_.velocity[0] = velocity_q1;
    command_message_.velocity[1] = velocity_q2;
    // SensorDataQoS는 best-effort/작은 history로 오래된 고주기 명령보다 최신 표본을 우선한다.
    command_pub_->publish(command_message_);
    samples_.fetch_add(1, std::memory_order_relaxed);
  }

  double minimum_clearance(double q1, double q2) const
  {
    double clearance = 10.0;
    for (std::size_t i = 0; i < active_world_.count; ++i) {
      // signed clearance=||q-c||-r. 양수는 obstacle 밖, 음수는 내부를 뜻한다.
      clearance = std::min(
        clearance,
        std::hypot(q1 - active_world_.q1[i], q2 - active_world_.q2[i]) -
        active_world_.radius[i]);
    }
    return clearance;
  }

  // 저우선순위 역할: 로그/직렬화 같은 느린 작업을 흉내 내고 원자 계측값만 읽어 진단을 발행한다.
  void background_diagnostics()
  {
    const auto busy_until = std::chrono::steady_clock::now() + 18ms;
    double dummy = 0.0;
    while (std::chrono::steady_clock::now() < busy_until) {
      dummy += std::sin(dummy + 0.01);
    }
    if (!std::isfinite(dummy)) {
      RCLCPP_DEBUG(get_logger(), "unreachable dummy=%f", dummy);
    }
    const auto low_callbacks = background_callbacks_.fetch_add(1, std::memory_order_relaxed) + 1;

    std_msgs::msg::String diagnostics;
    std::ostringstream stream;
    stream << std::boolalpha
           << "mailbox_lock_free=" << trajectory_mailbox_.is_lock_free()
           << " samples=" << samples_.load(std::memory_order_relaxed)
           << " max_period_jitter_us=" << max_period_jitter_us_.load(std::memory_order_relaxed)
           << " snapshot_misses=" << snapshot_misses_.load(std::memory_order_relaxed)
           << " low_callbacks=" << low_callbacks
           << " min_clearance_rad=" << min_clearance_.load(std::memory_order_relaxed)
           << " reached=" << reached_.load(std::memory_order_relaxed);
    diagnostics.data = stream.str();
    diagnostics_pub_->publish(diagnostics);
  }

  static void update_max(std::atomic<std::uint64_t> & destination, std::uint64_t candidate)
  {
    auto current = destination.load(std::memory_order_relaxed);
    while (current < candidate &&
      !destination.compare_exchange_weak(current, candidate, std::memory_order_relaxed))
    {
    }
  }

  rclcpp::CallbackGroup::SharedPtr control_group_;
  rclcpp::CallbackGroup::SharedPtr background_group_;
  rclcpp::TimerBase::SharedPtr control_timer_;
  rclcpp::TimerBase::SharedPtr diagnostics_timer_;
  rclcpp::Subscription<trajectory_msgs::msg::JointTrajectory>::SharedPtr trajectory_sub_;
  rclcpp::Subscription<daily_robotics_2026_09_15::msg::JointSpaceWorld>::SharedPtr world_sub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr command_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diagnostics_pub_;

  AtomicTrajectoryMailbox trajectory_mailbox_;
  AtomicWorldMailbox world_mailbox_;
  TrajectorySnapshot active_trajectory_;
  WorldSnapshot active_world_;
  sensor_msgs::msg::JointState command_message_;
  std::size_t active_index_{0};
  double current_q1_{0.0};
  double current_q2_{0.0};
  std::chrono::steady_clock::time_point previous_tick_{};
  bool have_previous_tick_{false};

  std::atomic<std::uint64_t> samples_{0};
  std::atomic<std::uint64_t> snapshot_misses_{0};
  std::atomic<std::uint64_t> background_callbacks_{0};
  std::atomic<std::uint64_t> max_period_jitter_us_{0};
  std::atomic<double> min_clearance_{std::numeric_limits<double>::max()};
  std::atomic<bool> reached_{false};
};

// Linux RT 권한이 있으면 control executor thread를 SCHED_FIFO로 올리고, 없으면 기능을 유지하며 경고한다.
void try_enable_fifo(const rclcpp::Logger & logger)
{
  sched_param parameters{};
  parameters.sched_priority = 20;
  const int result = pthread_setschedparam(pthread_self(), SCHED_FIFO, &parameters);
  if (result == 0) {
    RCLCPP_INFO(logger, "control executor: SCHED_FIFO priority=20");
  } else {
    RCLCPP_WARN(
      logger, "SCHED_FIFO unavailable (%s); callback isolation remains, hard RT is not claimed",
      std::strerror(result));
  }
}

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<daily_robotics::PrioritySafeServoNode>();

  // 각 SingleThreadedExecutor는 한 callback group만 담당한다. 두 group 사이에는 공유 mutex가 없다.
  rclcpp::executors::SingleThreadedExecutor control_executor;
  rclcpp::executors::SingleThreadedExecutor background_executor;
  control_executor.add_callback_group(node->control_group(), node->get_node_base_interface());
  background_executor.add_callback_group(node->background_group(), node->get_node_base_interface());

  RCLCPP_INFO(node->get_logger(), "atomic mailbox lock_free=%s", node->mailbox_lock_free() ? "true" : "false");
  std::jthread control_thread([&control_executor, &node]() {
      daily_robotics::try_enable_fifo(node->get_logger());
      control_executor.spin();
    });
  background_executor.spin();
  control_executor.cancel();
  rclcpp::shutdown();
  return 0;
}
