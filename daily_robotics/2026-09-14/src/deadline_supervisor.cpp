#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/header.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

/// planner heartbeat의 150 ms deadline을 독립 executor에서 감시하고 안전 속도만 통과시키는 노드다.
class DeadlineSupervisor final : public rclcpp::Node
{
public:
  DeadlineSupervisor()
  : Node("deadline_supervisor")
  {
    // 자동 등록을 끄면 main에서 critical/telemetry callback group을 서로 다른 executor에 배치할 수 있다.
    critical_group_ = create_callback_group(
      rclcpp::CallbackGroupType::MutuallyExclusive, false);
    telemetry_group_ = create_callback_group(
      rclcpp::CallbackGroupType::MutuallyExclusive, false);

    rclcpp::SubscriptionOptions critical_options;
    critical_options.callback_group = critical_group_;
    heartbeat_subscription_ = create_subscription<std_msgs::msg::Header>(
      "/planning/optimizer_heartbeat", rclcpp::QoS(rclcpp::KeepLast(4)).reliable(),
      [this](const std_msgs::msg::Header &) {
        // steady_clock은 /clock 변경이나 NTP 보정과 무관하므로 deadline 경과시간 측정에 적합하다.
        last_heartbeat_ns_.store(steady_now_ns(), std::memory_order_release);
        heartbeat_seen_.store(true, std::memory_order_release);
      }, critical_options);
    raw_command_subscription_ = create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel_raw", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
      [this](const geometry_msgs::msg::Twist & command) {
        latest_command_ = command;
        publish_safe_command();
      }, critical_options);

    safe_command_publisher_ = create_publisher<geometry_msgs::msg::Twist>(
      "/cmd_vel", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());
    stop_publisher_ = create_publisher<std_msgs::msg::Bool>(
      "/safety/stop", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
    diagnostics_publisher_ = create_publisher<std_msgs::msg::String>(
      "/safety/deadline_status",
      rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());

    // 20 ms 감시 주기는 150 ms deadline보다 충분히 짧아 검출 지연 상한을 약 20 ms로 제한한다.
    deadline_timer_ = create_wall_timer(20ms, [this]() {check_deadline();}, critical_group_);
    telemetry_timer_ = create_wall_timer(500ms, [this]() {publish_diagnostics();}, telemetry_group_);
  }

  rclcpp::CallbackGroup::SharedPtr critical_group() const {return critical_group_;}
  rclcpp::CallbackGroup::SharedPtr telemetry_group() const {return telemetry_group_;}

private:
  static std::int64_t steady_now_ns()
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  /// heartbeat age가 150 ms를 넘는 순간 stop을 latch하고, 새 heartbeat에서 자동 복구한다.
  void check_deadline()
  {
    const bool seen = heartbeat_seen_.load(std::memory_order_acquire);
    const auto age_ns = steady_now_ns() - last_heartbeat_ns_.load(std::memory_order_acquire);
    const bool should_stop = !seen || age_ns > 150'000'000LL;
    const bool was_stopped = stopped_.exchange(should_stop, std::memory_order_acq_rel);

    if (should_stop != was_stopped) {
      if (should_stop) {
        ++trip_count_;
      } else if (was_stopped && trip_count_ > 0U) {
        ++recovery_count_;
      }
      std_msgs::msg::Bool stop;
      stop.data = should_stop;
      stop_publisher_->publish(stop);
      publish_safe_command();
    }
  }

  /// stop 상태면 Twist 기본값(모든 축 0), 정상 상태면 최신 raw command를 내보낸다.
  void publish_safe_command()
  {
    geometry_msgs::msg::Twist safe_command;
    if (!stopped_.load(std::memory_order_acquire)) {
      safe_command = latest_command_;
    }
    safe_command_publisher_->publish(safe_command);
  }

  /// 저주기 문자열 조립을 별도 executor로 보내 critical 감시 callback의 실행 시간을 격리한다.
  void publish_diagnostics()
  {
    const auto age_ms = heartbeat_seen_.load(std::memory_order_acquire) ?
      (steady_now_ns() - last_heartbeat_ns_.load(std::memory_order_acquire)) / 1'000'000LL : -1LL;
    std_msgs::msg::String diagnostic;
    std::ostringstream text;
    text << "stopped=" << (stopped_.load(std::memory_order_acquire) ? "true" : "false")
         << " heartbeat_age_ms=" << age_ms
         << " deadline_ms=150 trips=" << trip_count_ << " recoveries=" << recovery_count_;
    diagnostic.data = text.str();
    diagnostics_publisher_->publish(diagnostic);
  }

  rclcpp::CallbackGroup::SharedPtr critical_group_;
  rclcpp::CallbackGroup::SharedPtr telemetry_group_;
  rclcpp::Subscription<std_msgs::msg::Header>::SharedPtr heartbeat_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr raw_command_subscription_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr safe_command_publisher_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stop_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diagnostics_publisher_;
  rclcpp::TimerBase::SharedPtr deadline_timer_;
  rclcpp::TimerBase::SharedPtr telemetry_timer_;
  geometry_msgs::msg::Twist latest_command_;
  std::atomic<std::int64_t> last_heartbeat_ns_{0LL};
  std::atomic<bool> heartbeat_seen_{false};
  std::atomic<bool> stopped_{true};
  std::uint64_t trip_count_{0U};
  std::uint64_t recovery_count_{0U};
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<daily_robotics::DeadlineSupervisor>();

  // 두 SingleThreadedExecutor는 critical 감시와 문자열 telemetry가 같은 작업 큐에서 경합하지 않게 한다.
  rclcpp::executors::SingleThreadedExecutor critical_executor;
  rclcpp::executors::SingleThreadedExecutor telemetry_executor;
  critical_executor.add_callback_group(node->critical_group(), node->get_node_base_interface());
  telemetry_executor.add_callback_group(node->telemetry_group(), node->get_node_base_interface());

  // std::jthread는 scope 종료 시 join되어 executor 보조 스레드의 수명 누수를 방지하는 C++20 RAII API다.
  std::jthread telemetry_thread([&telemetry_executor]() {telemetry_executor.spin();});
  critical_executor.spin();
  rclcpp::shutdown();
  return 0;
}
