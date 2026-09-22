#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>

#include "daily_robotics_2026_09_23/msg/effort_command.hpp"
#include "daily_robotics_2026_09_23/msg/safety_status.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr double kMass = 1.0;
constexpr double kJointDamping = 1.2;
constexpr double kWallPosition = 1.0;
constexpr double kWallStiffness = 250.0;
constexpr double kAllowedEffortLimit = 15.0;
constexpr double kRawFaultThreshold = 18.0;
constexpr double kForceFaultThreshold = 12.0;
constexpr double kEnergyTolerance = 0.40;
}  // namespace

/**
 * @brief 제어기와 독립적으로 힘·명령 나이·에너지 수지를 검사하고 허용 명령만 플랜트에 전달한다.
 *
 * 이 노드는 안전 릴레이의 소프트웨어 학습 모델이다. 세 샘플 연속 위반일 때 정지하고, 400 ms의
 * 최소 정지 시간과 50개 연속 정상 샘플을 모두 만족해야 복구한다. 실제 산업 안전은 별도 인증
 * 하드웨어/STO가 필요하며 이 예제만으로 사람과 장비의 안전을 보증할 수 없다.
 */
class EnergySafetyAuditor final : public rclcpp::Node
{
public:
  EnergySafetyAuditor()
  : Node("energy_safety_auditor"), start_(std::chrono::steady_clock::now())
  {
    allowed_pub_ = create_publisher<std_msgs::msg::Float64>(
      "/joint_effort_allowed", rclcpp::QoS(4).reliable());
    status_pub_ = create_publisher<daily_robotics_2026_09_23::msg::SafetyStatus>(
      "/contact/safety_status", rclcpp::QoS(4).reliable());
    // transient_local은 늦게 시작한 smoke test도 마지막 판정을 즉시 받을 수 있게 한다.
    audit_pub_ = create_publisher<std_msgs::msg::String>(
      "/contact/audit", rclcpp::QoS(1).reliable().transient_local());

    joint_sub_ = create_subscription<sensor_msgs::msg::JointState>(
      "/contact/joint_state", rclcpp::SensorDataQoS(),
      [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
        if (msg->position.size() == 1 && msg->velocity.size() == 1) {
          position_ = msg->position[0];
          velocity_ = msg->velocity[0];
          have_state_ = true;
        }
      });
    wrench_sub_ = create_subscription<geometry_msgs::msg::WrenchStamped>(
      "/contact/wrench", rclcpp::SensorDataQoS(),
      [this](const geometry_msgs::msg::WrenchStamped::SharedPtr msg) {
        measured_force_ = msg->wrench.force.x;
        saw_contact_ = saw_contact_ || std::abs(measured_force_) > 2.0;
        max_force_ = std::max(max_force_, std::abs(measured_force_));
      });
    raw_command_sub_ = create_subscription<daily_robotics_2026_09_23::msg::EffortCommand>(
      "/joint_effort_raw", rclcpp::QoS(4).reliable(),
      std::bind(&EnergySafetyAuditor::on_raw_command, this, std::placeholders::_1));

    // 문자열 조립과 로깅은 500 Hz 명령 콜백에서 분리해 2 Hz 저주기 경계에서만 수행한다.
    report_timer_ = create_wall_timer(500ms, std::bind(&EnergySafetyAuditor::publish_audit, this));
  }

private:
  /** @brief 한 명령을 검증하고 정지 히스테리시스를 적용한 뒤 허용 힘과 수치 상태를 발행한다. */
  void on_raw_command(const daily_robotics_2026_09_23::msg::EffortCommand::SharedPtr msg)
  {
    const auto callback_begin = std::chrono::steady_clock::now();
    const auto steady_now = callback_begin;
    const std::int64_t now_ns = now().nanoseconds();
    const std::int64_t msg_ns =
      static_cast<std::int64_t>(msg->stamp.sec) * 1000000000LL +
      static_cast<std::int64_t>(msg->stamp.nanosec);
    const double age_ms = std::max(0.0, static_cast<double>(now_ns - msg_ns) * 1.0e-6);

    const double dt = last_command_stamp_ns_ > 0 ?
      std::clamp(static_cast<double>(msg_ns - last_command_stamp_ns_) * 1.0e-9, 0.0005, 0.010) : 0.002;
    last_command_stamp_ns_ = msg_ns;

    // E = 1/2*m*v^2 + 1/2*K_wall*x_pen^2 는 플랜트의 저장 에너지다.
    const double penetration = std::max(0.0, position_ - kWallPosition);
    const double stored_energy =
      0.5 * kMass * velocity_ * velocity_ +
      0.5 * kWallStiffness * penetration * penetration;
    if (!energy_initialized_ && have_state_) {
      initial_energy_ = stored_energy;
      energy_initialized_ = true;
    }

    const bool finite = std::isfinite(msg->effort) && std::isfinite(measured_force_) &&
      std::isfinite(position_) && std::isfinite(velocity_);
    const bool raw_violation = !finite || std::abs(msg->effort) > kRawFaultThreshold ||
      std::abs(measured_force_) > kForceFaultThreshold || age_ms > 25.0;

    if (raw_violation) {
      ++consecutive_bad_;
      consecutive_good_ = 0;
    } else {
      consecutive_bad_ = 0;
      ++consecutive_good_;
    }

    if (!stopped_ && consecutive_bad_ >= 3) {
      stopped_ = true;
      saw_stop_ = true;
      ++trip_count_;
      recovery_not_before_ = steady_now + 400ms;
      consecutive_good_ = 0;
    }
    if (stopped_ && steady_now >= recovery_not_before_ && consecutive_good_ >= 50) {
      stopped_ = false;
      saw_recovery_ = true;
      ++recovery_count_;
      consecutive_bad_ = 0;
    }

    // 안전 게이트: 정상일 때도 물리 한계 ±15 N을 적용하고, 정지 상태에서는 정확히 0 N만 허용한다.
    const double allowed_effort = stopped_ || !finite ?
      0.0 : std::clamp(msg->effort, -kAllowedEffortLimit, kAllowedEffortLimit);

    // 수동성 수지: E(t)-E(0) <= ∫tau*v dt - ∫b*v^2 dt.
    // residual = 좌변-우변이므로 양의 큰 값은 모델 밖 에너지가 생겼다는 경고다.
    supplied_energy_ += allowed_effort * velocity_ * dt;
    dissipated_energy_ += kJointDamping * velocity_ * velocity_ * dt;
    const double energy_residual = energy_initialized_ ?
      stored_energy - initial_energy_ - supplied_energy_ + dissipated_energy_ : 0.0;
    max_energy_residual_ = std::max(max_energy_residual_, energy_residual);
    energy_ok_ = energy_ok_ && energy_residual <= kEnergyTolerance;
    max_controller_us_ = std::max(max_controller_us_, msg->hot_path_us);

    std_msgs::msg::Float64 allowed;
    allowed.data = allowed_effort;
    allowed_pub_->publish(allowed);

    const auto callback_finish = std::chrono::steady_clock::now();
    const double callback_us =
      std::chrono::duration<double, std::micro>(callback_finish - callback_begin).count();
    max_callback_us_ = std::max(max_callback_us_, callback_us);

    // 상태 메시지는 10샘플마다(약 50 Hz)만 발행해 500 Hz 안전 판단과 직렬화 비용을 분리한다.
    if ((msg->sequence % 10U) == 0U) {
      daily_robotics_2026_09_23::msg::SafetyStatus status;
      status.stamp = msg->stamp;
      status.sequence = msg->sequence;
      status.raw_effort = msg->effort;
      status.allowed_effort = allowed_effort;
      status.measured_contact_force = measured_force_;
      status.energy_residual = energy_residual;
      status.sample_age_ms = age_ms;
      status.stopped = stopped_;
      status.trip_count = trip_count_;
      status.recovery_count = recovery_count_;
      status.max_controller_us = max_controller_us_;
      status.max_callback_us = max_callback_us_;
      status_pub_->publish(status);
    }
  }

  /** @brief 통합 시험이 기계적으로 판정할 수 있는 누적 결과를 transient-local 문자열로 발행한다. */
  void publish_audit()
  {
    const double elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - start_).count();
    if (elapsed < 7.0) {
      return;
    }
    const bool pass = saw_contact_ && saw_stop_ && saw_recovery_ && energy_ok_ &&
      trip_count_ == 1U && recovery_count_ == 1U;
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3)
           << (pass ? "PASS" : "FAIL")
           << " contact=" << static_cast<int>(saw_contact_)
           << " stop=" << static_cast<int>(saw_stop_)
           << " recovery=" << static_cast<int>(saw_recovery_)
           << " force_n=" << max_force_
           << " energy_j=" << max_energy_residual_;
    std_msgs::msg::String audit;
    audit.data = stream.str();
    audit_pub_->publish(audit);
  }

  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr allowed_pub_;
  rclcpp::Publisher<daily_robotics_2026_09_23::msg::SafetyStatus>::SharedPtr status_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_pub_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_sub_;
  rclcpp::Subscription<geometry_msgs::msg::WrenchStamped>::SharedPtr wrench_sub_;
  rclcpp::Subscription<daily_robotics_2026_09_23::msg::EffortCommand>::SharedPtr raw_command_sub_;
  rclcpp::TimerBase::SharedPtr report_timer_;

  std::chrono::steady_clock::time_point start_;
  std::chrono::steady_clock::time_point recovery_not_before_{};
  std::int64_t last_command_stamp_ns_{0};
  double position_{0.0};
  double velocity_{0.0};
  double measured_force_{0.0};
  double initial_energy_{0.0};
  double supplied_energy_{0.0};
  double dissipated_energy_{0.0};
  double max_energy_residual_{0.0};
  double max_force_{0.0};
  double max_controller_us_{0.0};
  double max_callback_us_{0.0};
  std::uint32_t consecutive_bad_{0};
  std::uint32_t consecutive_good_{0};
  std::uint32_t trip_count_{0};
  std::uint32_t recovery_count_{0};
  bool have_state_{false};
  bool energy_initialized_{false};
  bool stopped_{false};
  bool saw_contact_{false};
  bool saw_stop_{false};
  bool saw_recovery_{false};
  bool energy_ok_{true};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // 단일 스레드 spin은 상태/힘/명령 콜백 순서를 직렬화한다. 실제 다중 스레드 RT 구성에서는
  // 이 공유 상태를 lock-free snapshot이나 realtime_tools::RealtimeBuffer로 교체해야 한다.
  rclcpp::spin(std::make_shared<EnergySafetyAuditor>());
  rclcpp::shutdown();
  return 0;
}
