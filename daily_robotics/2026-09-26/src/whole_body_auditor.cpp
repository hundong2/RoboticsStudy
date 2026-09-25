#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "daily_robotics_2026_09_26/msg/contact_solution.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/string.hpp"

namespace daily_robotics
{

using ContactSolution = daily_robotics_2026_09_26::msg::ContactSolution;

// 이 노드는 QP 구현과 독립적으로 wrench 재구성, 마찰 제약, Jacobian transpose 토크를 검사한다.
// 제어기 스스로 만든 feasible 플래그만 믿지 않고 별도 노드가 안전 계약을 다시 계산하는 구조다.
class WholeBodyAuditor final : public rclcpp::Node
{
public:
  WholeBodyAuditor()
  : Node("whole_body_auditor")
  {
    friction_coefficient_ = declare_parameter<double>("friction_coefficient", 0.6);
    min_normal_force_ = declare_parameter<double>("min_normal_force", 15.0);
    max_normal_force_ = declare_parameter<double>("max_normal_force", 180.0);
    contact_half_span_ = declare_parameter<double>("contact_half_span", 0.25);
    com_height_ = declare_parameter<double>("com_height", 0.55);

    const auto state_qos = rclcpp::QoS(rclcpp::KeepLast(5)).reliable();
    solution_subscription_ = create_subscription<ContactSolution>(
      "/wbc/contact_solution", state_qos,
      [this](ContactSolution::ConstSharedPtr solution) {audit(*solution);});

    // JointState effort에는 tau=J^T f로 얻은 네 관절 토크를 담아 표준 ROS 도구에서 볼 수 있게 한다.
    joint_effort_publisher_ = create_publisher<sensor_msgs::msg::JointState>(
      "/wbc/joint_effort", state_qos);
    safe_publisher_ = create_publisher<std_msgs::msg::Bool>("/wbc/safe", state_qos);

    // transient_local은 smoke test가 늦게 구독해도 마지막 종합 감사 결과를 즉시 받을 수 있게 한다.
    const auto audit_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    audit_publisher_ = create_publisher<std_msgs::msg::String>("/wbc/audit", audit_qos);
  }

private:
  double contact_violation(double tangent, double normal) const noexcept
  {
    return std::max({
      0.0,
      min_normal_force_ - normal,
      normal - max_normal_force_,
      std::abs(tangent) - friction_coefficient_ * normal});
  }

  // 평면 2R 다리에서 발에 작용하는 힘을 두 관절의 일반화 토크로 바꾼다.
  std::array<double, 2> jacobian_transpose_torque(
    double hip_angle, double knee_angle, double fx, double fz) const noexcept
  {
    constexpr double thigh_length = 0.32;
    constexpr double shank_length = 0.32;
    const double combined = hip_angle + knee_angle;

    // 발 위치 x=l1*sin(q1)+l2*sin(q1+q2), z=-l1*cos(q1)-l2*cos(q1+q2)를
    // q로 미분하면 아래 2x2 geometric Jacobian J가 된다.
    const double j_x_hip =
      thigh_length * std::cos(hip_angle) + shank_length * std::cos(combined);
    const double j_x_knee = shank_length * std::cos(combined);
    const double j_z_hip =
      thigh_length * std::sin(hip_angle) + shank_length * std::sin(combined);
    const double j_z_knee = shank_length * std::sin(combined);

    // 가상일 원리 tau^T dq = f^T dx와 dx=J dq를 결합하면 tau=J^T f가 된다.
    return {{j_x_hip * fx + j_z_hip * fz, j_x_knee * fx + j_z_knee * fz}};
  }

  void audit(const ContactSolution & solution)
  {
    // allocator와 별도의 코드로 w=A f를 다시 계산해 메시지/행렬/부호 오류를 잡는다.
    const double achieved_fx = solution.left_fx + solution.right_fx;
    const double achieved_fz = solution.left_fz + solution.right_fz;
    const double achieved_tau_y =
      -com_height_ * (solution.left_fx + solution.right_fx) +
      contact_half_span_ * solution.left_fz - contact_half_span_ * solution.right_fz;
    const double error_fx = achieved_fx - solution.desired_fx;
    const double error_fz = achieved_fz - solution.desired_fz;
    const double error_tau = achieved_tau_y - solution.desired_tau_y;
    const double residual = std::sqrt(
      error_fx * error_fx + error_fz * error_fz + error_tau * error_tau);

    const double violation = std::max(
      contact_violation(solution.left_fx, solution.left_fz),
      contact_violation(solution.right_fx, solution.right_fz));
    const double reported_wrench_mismatch = std::max({
      std::abs(achieved_fx - solution.achieved_fx),
      std::abs(achieved_fz - solution.achieved_fz),
      std::abs(achieved_tau_y - solution.achieved_tau_y)});

    // 두 발 자세를 다르게 두어 같은 접촉력도 다리 형상에 따라 토크가 달라짐을 보여 준다.
    const auto left_tau = jacobian_transpose_torque(
      -0.35, 0.70, solution.left_fx, solution.left_fz);
    const auto right_tau = jacobian_transpose_torque(
      0.35, -0.70, solution.right_fx, solution.right_fz);
    const bool jacobian_tau_finite =
      std::isfinite(left_tau[0]) && std::isfinite(left_tau[1]) &&
      std::isfinite(right_tau[0]) && std::isfinite(right_tau[1]);

    sensor_msgs::msg::JointState joint_state;
    joint_state.header.stamp = now();
    // JointState의 name과 effort는 같은 인덱스가 같은 관절을 뜻해야 한다.
    joint_state.name = {
      "left_hip_pitch", "left_knee_pitch", "right_hip_pitch", "right_knee_pitch"};
    joint_state.effort = {left_tau[0], left_tau[1], right_tau[0], right_tau[1]};
    joint_effort_publisher_->publish(joint_state);

    // 196.2 N 체중 지지 문제에서 혼합 단위 residual 1.0 미만(약 0.51%)을 실습 통과 기준으로 둔다.
    const bool sample_safe = solution.feasible && violation <= 1.0e-9 &&
      residual < 1.0 && reported_wrench_mismatch < 1.0e-9 && jacobian_tau_finite;
    std_msgs::msg::Bool safe_message;
    safe_message.data = sample_safe;
    safe_publisher_->publish(safe_message);

    ++sample_count_;
    all_samples_safe_ = all_samples_safe_ && sample_safe;
    bounded_iterations_ = bounded_iterations_ &&
      solution.iterations >= 8U && solution.iterations <= 128U;
    all_indices_lock_free_ = all_indices_lock_free_ && solution.atomic_indices_lock_free;
    max_residual_ = std::max(max_residual_, residual);
    max_violation_ = std::max(max_violation_, violation);
    max_solve_time_us_ = std::max(max_solve_time_us_, solution.solve_time_us);
    min_desired_fx_ = std::min(min_desired_fx_, solution.desired_fx);
    max_desired_fx_ = std::max(max_desired_fx_, solution.desired_fx);
    jacobian_tau_finite_ = jacobian_tau_finite_ && jacobian_tau_finite;

    // 최소 40개 상태와 5 N 이상의 명령 변화를 본 뒤에만 통합 PASS를 발행한다.
    // 한 정적 샘플만 맞는 구현이 우연히 통과하지 않게 하는 간단한 excitation 조건이다.
    if (sample_count_ >= 40U && max_desired_fx_ - min_desired_fx_ > 5.0) {
      const bool pass = all_samples_safe_ && bounded_iterations_ && jacobian_tau_finite_;
      std_msgs::msg::String audit_message;
      std::ostringstream stream;
      stream << (pass ? "AUDIT_PASS" : "AUDIT_FAIL")
             << " samples=" << sample_count_
             << " residual_max=" << max_residual_
             << " violation_max=" << max_violation_
             << " solve_us_max=" << max_solve_time_us_
             << " bounded_iters=" << (bounded_iterations_ ? 1 : 0)
             << " jacobian_tau_finite=" << (jacobian_tau_finite_ ? 1 : 0)
             << " lock_free_indices=" << (all_indices_lock_free_ ? 1 : 0);
      audit_message.data = stream.str();
      audit_publisher_->publish(audit_message);

      RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 2000, "%s", audit_message.data.c_str());
    }
  }

  double friction_coefficient_{0.6};
  double min_normal_force_{15.0};
  double max_normal_force_{180.0};
  double contact_half_span_{0.25};
  double com_height_{0.55};

  std::uint64_t sample_count_{0U};
  bool all_samples_safe_{true};
  bool bounded_iterations_{true};
  bool all_indices_lock_free_{true};
  bool jacobian_tau_finite_{true};
  double max_residual_{0.0};
  double max_violation_{0.0};
  double max_solve_time_us_{0.0};
  double min_desired_fx_{std::numeric_limits<double>::infinity()};
  double max_desired_fx_{-std::numeric_limits<double>::infinity()};

  rclcpp::Subscription<ContactSolution>::SharedPtr solution_subscription_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_effort_publisher_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr safe_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr audit_publisher_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<daily_robotics::WholeBodyAuditor>());
  rclcpp::shutdown();
  return 0;
}
