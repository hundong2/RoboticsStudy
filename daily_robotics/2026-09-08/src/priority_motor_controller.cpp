#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <thread>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

namespace
{
using namespace std::chrono_literals;

constexpr double kWheelRadius = 0.08;
constexpr double kWheelSeparation = 0.42;
constexpr double kControlPeriodSeconds = 0.01;
constexpr std::int64_t kCommandTimeoutNanoseconds = 500'000'000;

std::uint64_t double_to_bits(double value)
{
  // C++17에는 std::bit_cast가 없으므로 memcpy로 double 비트열을 uint64_t에 안전하게 복사한다.
  static_assert(sizeof(double) == sizeof(std::uint64_t), "64-bit double is required");
  std::uint64_t bits{};
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

double bits_to_double(std::uint64_t bits)
{
  double value{};
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::int64_t steady_now_nanoseconds()
{
  // steady_clock은 시스템 시각 보정과 무관하게 단조 증가하므로 command timeout 판정에 적합하다.
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct CommandSnapshot
{
  double linear{0.0};
  double angular{0.0};
  std::int64_t received_at_nanoseconds{0};
};

struct WheelSnapshot
{
  double left_position{0.0};
  double right_position{0.0};
  std::int64_t sampled_at_nanoseconds{0};
};

class AtomicCommandBuffer
{
public:
  void write(const CommandSnapshot & value)
  {
    // sequence가 홀수인 동안 writer가 갱신 중이고, 짝수이면 완전한 snapshot이다.
    sequence_.fetch_add(1U, std::memory_order_acq_rel);
    linear_bits_.store(double_to_bits(value.linear), std::memory_order_relaxed);
    angular_bits_.store(double_to_bits(value.angular), std::memory_order_relaxed);
    stamp_.store(value.received_at_nanoseconds, std::memory_order_relaxed);
    sequence_.fetch_add(1U, std::memory_order_release);
  }

  CommandSnapshot read() const
  {
    // 모든 payload도 atomic이라 C++ memory model상 data race가 없다. 전후 sequence가 같을 때만
    // linear/angular/stamp가 동일한 write transaction에서 왔다고 인정한다.
    for (;;) {
      const std::uint64_t before = sequence_.load(std::memory_order_acquire);
      if ((before & 1U) != 0U) {
        continue;
      }

      CommandSnapshot snapshot;
      snapshot.linear = bits_to_double(linear_bits_.load(std::memory_order_relaxed));
      snapshot.angular = bits_to_double(angular_bits_.load(std::memory_order_relaxed));
      snapshot.received_at_nanoseconds = stamp_.load(std::memory_order_relaxed);

      const std::uint64_t after = sequence_.load(std::memory_order_acquire);
      if (before == after) {
        return snapshot;
      }
    }
  }

  bool is_lock_free() const
  {
    return sequence_.is_lock_free() && linear_bits_.is_lock_free() &&
           angular_bits_.is_lock_free() && stamp_.is_lock_free();
  }

private:
  alignas(64) std::atomic<std::uint64_t> sequence_{0U};
  std::atomic<std::uint64_t> linear_bits_{double_to_bits(0.0)};
  std::atomic<std::uint64_t> angular_bits_{double_to_bits(0.0)};
  std::atomic<std::int64_t> stamp_{0};
};

class AtomicWheelBuffer
{
public:
  void write(const WheelSnapshot & value)
  {
    sequence_.fetch_add(1U, std::memory_order_acq_rel);
    left_bits_.store(double_to_bits(value.left_position), std::memory_order_relaxed);
    right_bits_.store(double_to_bits(value.right_position), std::memory_order_relaxed);
    stamp_.store(value.sampled_at_nanoseconds, std::memory_order_relaxed);
    sequence_.fetch_add(1U, std::memory_order_release);
  }

  WheelSnapshot read() const
  {
    for (;;) {
      const std::uint64_t before = sequence_.load(std::memory_order_acquire);
      if ((before & 1U) != 0U) {
        continue;
      }

      WheelSnapshot snapshot;
      snapshot.left_position = bits_to_double(left_bits_.load(std::memory_order_relaxed));
      snapshot.right_position = bits_to_double(right_bits_.load(std::memory_order_relaxed));
      snapshot.sampled_at_nanoseconds = stamp_.load(std::memory_order_relaxed);

      const std::uint64_t after = sequence_.load(std::memory_order_acquire);
      if (before == after) {
        return snapshot;
      }
    }
  }

private:
  alignas(64) std::atomic<std::uint64_t> sequence_{0U};
  std::atomic<std::uint64_t> left_bits_{double_to_bits(0.0)};
  std::atomic<std::uint64_t> right_bits_{double_to_bits(0.0)};
  std::atomic<std::int64_t> stamp_{0};
};
}  // namespace

class PriorityMotorController : public rclcpp::Node
{
public:
  PriorityMotorController()
  : Node("priority_motor_controller")
  {
    // 서로 다른 MutuallyExclusive callback group은 executor 단위로 제어/I/O를 분리하는 손잡이다.
    control_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    io_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions command_options;
    command_options.callback_group = io_group_;
    command_subscription_ = create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
      std::bind(&PriorityMotorController::on_command, this, std::placeholders::_1),
      command_options);

    // JointState는 좌/우 엔코더 누적 회전각(rad)과 각속도(rad/s)를 표현한다.
    joint_publisher_ = create_publisher<sensor_msgs::msg::JointState>(
      "/wheel_states", rclcpp::SensorDataQoS());
    joint_message_.name = {"left_wheel_joint", "right_wheel_joint"};
    joint_message_.position.resize(2U);
    joint_message_.velocity.resize(2U);

    // control timer는 전용 executor에서 100 Hz로 동작하며 DDS publish나 logging을 하지 않는다.
    control_timer_ = create_wall_timer(
      10ms, std::bind(&PriorityMotorController::control_tick, this), control_group_);
    // telemetry timer는 낮은 우선순위 I/O executor에서 50 Hz로 ROS 메시지를 만든다.
    telemetry_timer_ = create_wall_timer(
      20ms, std::bind(&PriorityMotorController::publish_wheel_state, this), io_group_);
  }

  rclcpp::CallbackGroup::SharedPtr control_group() const {return control_group_;}
  rclcpp::CallbackGroup::SharedPtr io_group() const {return io_group_;}
  bool atomic_path_is_lock_free() const {return command_buffer_.is_lock_free();}

private:
  void on_command(const geometry_msgs::msg::Twist::SharedPtr message)
  {
    // 이 I/O callback은 navigation의 차체 명령을 RT callback이 읽을 수 있는 일관된 snapshot으로 바꾼다.
    command_buffer_.write(
      CommandSnapshot{message->linear.x, message->angular.z, steady_now_nanoseconds()});
  }

  void control_tick()
  {
    // 이 callback은 실제 로봇의 100 Hz motor write/read loop에 해당한다.
    CommandSnapshot command = command_buffer_.read();
    const std::int64_t now_ns = steady_now_nanoseconds();
    if (command.received_at_nanoseconds == 0 ||
      now_ns - command.received_at_nanoseconds > kCommandTimeoutNanoseconds)
    {
      // 500 ms 동안 새 명령이 없으면 stale command를 계속 실행하지 않고 정지한다.
      command.linear = 0.0;
      command.angular = 0.0;
    }

    // 차동구동 역운동학:
    // omega_L=(v-omega*L/2)/r, omega_R=(v+omega*L/2)/r.
    const double left_velocity =
      (command.linear - command.angular * kWheelSeparation * 0.5) / kWheelRadius;
    const double right_velocity =
      (command.linear + command.angular * kWheelSeparation * 0.5) / kWheelRadius;

    left_position_ += left_velocity * kControlPeriodSeconds;
    right_position_ += right_velocity * kControlPeriodSeconds;
    last_left_velocity_.store(double_to_bits(left_velocity), std::memory_order_relaxed);
    last_right_velocity_.store(double_to_bits(right_velocity), std::memory_order_relaxed);
    wheel_buffer_.write(WheelSnapshot{left_position_, right_position_, now_ns});
  }

  void publish_wheel_state()
  {
    // DDS serialization/메시지 게시를 제어 thread 밖으로 밀어 priority inversion 가능성을 줄인다.
    const WheelSnapshot wheels = wheel_buffer_.read();
    joint_message_.header.stamp = now();
    joint_message_.position[0] = wheels.left_position;
    joint_message_.position[1] = wheels.right_position;
    joint_message_.velocity[0] = bits_to_double(
      last_left_velocity_.load(std::memory_order_relaxed));
    joint_message_.velocity[1] = bits_to_double(
      last_right_velocity_.load(std::memory_order_relaxed));
    joint_publisher_->publish(joint_message_);

    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 2000,
      "encoder(rad): left=%.3f right=%.3f", wheels.left_position, wheels.right_position);
  }

  rclcpp::CallbackGroup::SharedPtr control_group_;
  rclcpp::CallbackGroup::SharedPtr io_group_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr command_subscription_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_publisher_;
  rclcpp::TimerBase::SharedPtr control_timer_;
  rclcpp::TimerBase::SharedPtr telemetry_timer_;

  AtomicCommandBuffer command_buffer_;
  AtomicWheelBuffer wheel_buffer_;
  std::atomic<std::uint64_t> last_left_velocity_{double_to_bits(0.0)};
  std::atomic<std::uint64_t> last_right_velocity_{double_to_bits(0.0)};
  sensor_msgs::msg::JointState joint_message_;
  double left_position_{0.0};
  double right_position_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<PriorityMotorController>();

  // add_callback_group은 한 Node 전체가 아니라 선택한 callback group만 executor에 배치한다.
  rclcpp::executors::SingleThreadedExecutor control_executor;
  rclcpp::executors::SingleThreadedExecutor io_executor;
  control_executor.add_callback_group(node->control_group(), node->get_node_base_interface());
  io_executor.add_callback_group(node->io_group(), node->get_node_base_interface());

  RCLCPP_INFO(
    node->get_logger(), "atomic command path lock-free=%s",
    node->atomic_path_is_lock_free() ? "true" : "false");

  std::thread control_thread([&control_executor, &node]() {
      // SCHED_FIFO는 숫자가 큰 RT thread가 일반 SCHED_OTHER 작업보다 먼저 실행되게 요청한다.
      // 일반 container는 CAP_SYS_NICE가 없어 실패할 수 있으므로 실패를 숨기지 않고 기능은 계속 실행한다.
      sched_param policy_parameter{};
      policy_parameter.sched_priority = 60;
      const int scheduling_result =
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &policy_parameter);
      if (scheduling_result != 0) {
        RCLCPP_WARN(
          node->get_logger(),
          "SCHED_FIFO priority 60 unavailable (error=%d); executor isolation remains active",
          scheduling_result);
      } else {
        RCLCPP_INFO(node->get_logger(), "control executor uses SCHED_FIFO priority 60");
      }

      // spin은 control group의 ready timer만 가져와 현재 전용 thread에서 실행한다.
      control_executor.spin();
    });

  // main thread는 command subscription과 telemetry timer처럼 비-RT I/O callback만 처리한다.
  io_executor.spin();
  control_thread.join();
  rclcpp::shutdown();
  return 0;
}
