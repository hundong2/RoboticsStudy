#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

// 이 노드는 감독기 내부 상태를 공유하지 않는 독립 acceptance oracle이다. /diagnostics와
// /safe_command만 보고 정상→액추에이터 정지→회복→통신 정지→최종 회복 순서를 검증한다.
class SafetyAuditor final : public rclcpp::Node
{
public:
  SafetyAuditor()
  : Node("safety_auditor"), start_time_(std::chrono::steady_clock::now())
  {
    diagnostics_subscription_ = create_subscription<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local(),
      std::bind(&SafetyAuditor::on_diagnostics, this, std::placeholders::_1));
    safe_command_subscription_ = create_subscription<std_msgs::msg::Float64>(
      "/actuator/safe_command", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local(),
      std::bind(&SafetyAuditor::on_safe_command, this, std::placeholders::_1));
    audit_publisher_ = create_publisher<std_msgs::msg::String>(
      "/actuator/audit", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
    timer_ = create_wall_timer(100ms, std::bind(&SafetyAuditor::evaluate, this));
  }

private:
  // DiagnosticStatus.values에서 주어진 key의 문자열 값을 찾는다.
  static std::string value_of(
    const diagnostic_msgs::msg::DiagnosticStatus & status, const std::string & key)
  {
    for (const auto & item : status.values) {
      if (item.key == key) {
        return item.value;
      }
    }
    return {};
  }

  static double number_of(
    const diagnostic_msgs::msg::DiagnosticStatus & status, const std::string & key)
  {
    const std::string value = value_of(status, key);
    return value.empty() ? 0.0 : std::stod(value);
  }

  // 안전 명령을 별도 Topic에서 관찰해 상태 문자열만 바꾼 가짜 구현을 잡는다.
  void on_safe_command(const std_msgs::msg::Float64::SharedPtr message)
  {
    last_safe_command_ = message->data;
    safe_command_received_ = true;
  }

  // 진단 스냅샷에서 상태 전이의 순서와 수치 근거를 누적한다.
  void on_diagnostics(const diagnostic_msgs::msg::DiagnosticArray::SharedPtr message)
  {
    if (message->status.empty()) {
      return;
    }
    const auto & status = message->status.front();
    const std::string state = value_of(status, "state");
    const std::string reason = value_of(status, "reason");
    const double degraded_probability = number_of(status, "degraded_probability");
    const double sample_age_ms = number_of(status, "sample_age_ms");
    max_hot_path_us_ = std::max(
      max_hot_path_us_, number_of(status, "max_hot_path_us"));

    if (!saw_initial_normal_ && state == "NORMAL") {
      saw_initial_normal_ = true;
    }
    if (saw_initial_normal_ && state == "SAFE_STOP" && reason == "ACTUATOR" &&
      degraded_probability >= 0.85 && safe_command_received_ &&
      std::abs(last_safe_command_) <= 0.01)
    {
      saw_actuator_stop_ = true;
    }
    if (saw_actuator_stop_ && !saw_transport_stop_ && state == "NORMAL") {
      saw_actuator_recovery_ = true;
    }

    const double deadline_misses = number_of(status, "deadline_misses");
    const double liveliness_losses = number_of(status, "liveliness_losses");
    if (saw_actuator_recovery_ && state == "SAFE_STOP" && reason == "TRANSPORT" &&
      sample_age_ms >= 25.0 && (deadline_misses >= 1.0 || liveliness_losses >= 1.0) &&
      safe_command_received_ && std::abs(last_safe_command_) <= 0.01)
    {
      saw_transport_stop_ = true;
    }
    if (saw_transport_stop_ && state == "NORMAL") {
      saw_final_recovery_ = true;
    }
  }

  // 모든 관찰이 끝나면 한 번만 PASS를 내고, 14초까지 충족하지 못하면 상세 FAIL을 낸다.
  void evaluate()
  {
    if (published_) {
      return;
    }
    const double elapsed_s = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - start_time_).count();
    const bool timing_reasonable = max_hot_path_us_ > 0.0 && max_hot_path_us_ < 5000.0;
    const bool passed = saw_initial_normal_ && saw_actuator_stop_ &&
      saw_actuator_recovery_ && saw_transport_stop_ && saw_final_recovery_ && timing_reasonable;
    if (!passed && elapsed_s < 14.0) {
      return;
    }

    std::ostringstream stream;
    stream << (passed ? "PASS" : "FAIL")
           << " initial_normal=" << saw_initial_normal_
           << " actuator_stop=" << saw_actuator_stop_
           << " actuator_recovery=" << saw_actuator_recovery_
           << " transport_stop=" << saw_transport_stop_
           << " final_recovery=" << saw_final_recovery_
           << " max_hot_path_us=" << max_hot_path_us_;
    std_msgs::msg::String result;
    result.data = stream.str();
    audit_publisher_->publish(result);
    published_ = true;

    if (passed) {
      RCLCPP_INFO(get_logger(), "%s", result.data.c_str());
    } else {
      RCLCPP_ERROR(get_logger(), "%s", result.data.c_str());
    }
  }

  rclcpp::Subscription<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_subscription_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr safe_command_subscription_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;

  std::chrono::steady_clock::time_point start_time_;
  double last_safe_command_{0.0};
  double max_hot_path_us_{0.0};
  bool safe_command_received_{false};
  bool saw_initial_normal_{false};
  bool saw_actuator_stop_{false};
  bool saw_actuator_recovery_{false};
  bool saw_transport_stop_{false};
  bool saw_final_recovery_{false};
  bool published_{false};
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SafetyAuditor>());
  rclcpp::shutdown();
  return 0;
}
