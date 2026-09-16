#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>

#include "daily_robotics_2026_09_16/msg/calibration_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace daily_robotics
{

class CalibrationAuditor final : public rclcpp::Node
{
public:
  // 이 노드는 보정 알고리즘과 독립된 acceptance rule을 적용한다. 같은 프로세스의 내부 변수 대신
  // 공개 Topic만 관찰하므로 integration wiring과 QoS까지 함께 검증한다.
  CalibrationAuditor()
  : Node("calibration_auditor")
  {
    expected_offset_ms_ = declare_parameter<double>("expected_offset_ms", 35.0);

    // 보정 결과는 reliable + transient-local이므로 auditor가 늦게 시작해도 최신 snapshot을 받는다.
    const auto status_qos = rclcpp::QoS(1).reliable().transient_local();
    subscription_ = create_subscription<daily_robotics_2026_09_16::msg::CalibrationStatus>(
      "/calibration/status", status_qos,
      std::bind(&CalibrationAuditor::on_status, this, std::placeholders::_1));
    audit_publisher_ = create_publisher<std_msgs::msg::String>(
      "/calibration/audit", rclcpp::QoS(1).reliable().transient_local());
  }

private:
  // 연속 네 번 기준을 통과해야 PASS를 내므로 우연히 좋은 한 frame에 의한 오판을 줄인다.
  void on_status(const daily_robotics_2026_09_16::msg::CalibrationStatus::SharedPtr status)
  {
    const double offset_error_ms = std::abs(status->estimated_offset_ms - expected_offset_ms_);
    max_solve_time_us_ = std::max(max_solve_time_us_, status->solve_time_us);

    const bool offset_ok = offset_error_ms <= 3.0;
    const bool residual_ok = status->residual_rmse <= 0.060;
    const bool data_ok = status->matched_pairs >= 50U && status->imu_samples >= 500U;
    // 161개 후보는 [-80,+80] ms를 1 ms 간격으로 빠짐없이 검사했다는 계산 계약이다.
    const bool budget_shape_ok = status->search_candidates == 161U;
    const bool solve_budget_ok = status->solve_time_us <= 50'000U;
    const bool current_pass =
      offset_ok && residual_ok && data_ok && budget_shape_ok && solve_budget_ok;
    consecutive_passes_ = current_pass ? consecutive_passes_ + 1U : 0U;

    char buffer[320];
    const char * verdict = consecutive_passes_ >= 4U ? "PASS" : "WARMUP";
    // snprintf는 검증 내용을 key=value 한 줄로 고정해 smoke test와 사람이 모두 읽기 쉽게 한다.
    std::snprintf(
      buffer, sizeof(buffer),
      "%s offset_ms=%.3f error_ms=%.3f rmse=%.5f pairs=%u consecutive=%u "
      "solve_us=%lu max_solve_us=%lu",
      verdict, status->estimated_offset_ms, offset_error_ms, status->residual_rmse,
      status->matched_pairs, consecutive_passes_,
      static_cast<unsigned long>(status->solve_time_us),
      static_cast<unsigned long>(max_solve_time_us_));

    std_msgs::msg::String audit;
    audit.data = buffer;
    audit_publisher_->publish(audit);

    if (consecutive_passes_ == 4U) {
      RCLCPP_INFO(get_logger(), "Calibration acceptance passed: %s", buffer);
    }
  }

  double expected_offset_ms_{35.0};
  std::uint32_t consecutive_passes_{0U};
  std::uint64_t max_solve_time_us_{0U};
  rclcpp::Subscription<daily_robotics_2026_09_16::msg::CalibrationStatus>::SharedPtr subscription_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_publisher_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<daily_robotics::CalibrationAuditor>());
  rclcpp::shutdown();
  return 0;
}
