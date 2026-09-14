#include <chrono>
#include <memory>
#include <sstream>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

// 이 노드는 planner와 servo가 스스로 낸 진단을 독립적으로 모아 통합 실습의 성공 조건을 판정한다.
// 테스트가 단순히 process가 살아 있다는 사실이 아니라 "경로 생성+추종+RT 구조"를 확인하게 한다.
class TrajectoryAuditorNode final : public rclcpp::Node
{
public:
  TrajectoryAuditorNode()
  : Node("trajectory_auditor"), started_(std::chrono::steady_clock::now())
  {
    const auto latched_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    audit_pub_ = create_publisher<std_msgs::msg::String>("/planning/audit", latched_qos);

    // 각 구독 callback은 최신 진단 문자열을 보관한다. 기본 MutuallyExclusive group이라 별도 lock이 필요 없다.
    rrt_sub_ = create_subscription<std_msgs::msg::String>(
      "/planning/rrt_diagnostics", latched_qos,
      [this](std_msgs::msg::String::SharedPtr message) {rrt_diagnostics_ = message->data;});
    servo_sub_ = create_subscription<std_msgs::msg::String>(
      "/servo/diagnostics", latched_qos,
      [this](std_msgs::msg::String::SharedPtr message) {servo_diagnostics_ = message->data;});

    // 250 ms timer는 성공 조건을 빠르게 감지하되, 14초 후에는 명시적 실패를 남긴다.
    timer_ = create_wall_timer(250ms, std::bind(&TrajectoryAuditorNode::evaluate, this));
  }

private:
  void evaluate()
  {
    if (finished_) {
      return;
    }
    const bool planned = rrt_diagnostics_.find("status=SOLVED") != std::string::npos;
    const bool lock_free = servo_diagnostics_.find("mailbox_lock_free=true") != std::string::npos;
    const bool reached = servo_diagnostics_.find("reached=true") != std::string::npos;

    const auto elapsed = std::chrono::steady_clock::now() - started_;
    if (planned && lock_free && reached) {
      publish_result("PASS", "bounded_path_and_priority_safe_servo_completed");
    } else if (elapsed >= 14s) {
      publish_result("FAIL", "timeout_or_contract_violation");
    }
  }

  void publish_result(const std::string & status, const std::string & reason)
  {
    std_msgs::msg::String audit;
    std::ostringstream stream;
    stream << "status=" << status << " reason=" << reason
           << " | rrt={" << rrt_diagnostics_ << "}"
           << " | servo={" << servo_diagnostics_ << "}";
    audit.data = stream.str();
    audit_pub_->publish(audit);
    finished_ = true;
    RCLCPP_INFO(get_logger(), "%s", audit.data.c_str());
  }

  std::chrono::steady_clock::time_point started_;
  std::string rrt_diagnostics_;
  std::string servo_diagnostics_;
  bool finished_{false};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr rrt_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr servo_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<daily_robotics::TrajectoryAuditorNode>());
  rclcpp::shutdown();
  return 0;
}
