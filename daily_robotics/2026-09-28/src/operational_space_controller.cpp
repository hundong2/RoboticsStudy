#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"

#include "daily_robotics_2026_09_28/arm_model.hpp"
#include "daily_robotics_2026_09_28/msg/control_stats.hpp"

namespace study = daily_robotics_2026_09_28::model;
using ControlStats = daily_robotics_2026_09_28::msg::ControlStats;

// 이 노드는 JointState를 받을 때마다 말단 2D 추종 토크와 null-space 자세 토크를 계산한다.
// 로봇 시스템에서의 역할은 "손끝은 원 궤적을 따라가되 남는 1 자유도로 편한 자세를 유지"하는 것이다.
class OperationalSpaceController : public rclcpp::Node
{
public:
  OperationalSpaceController()
  : Node("operational_space_controller"),
    start_time_(std::chrono::steady_clock::now())
  {
    // 상태는 최신 샘플만 필요하므로 plant와 동일한 SensorDataQoS depth 1을 써야 QoS가 호환된다.
    const auto state_qos = rclcpp::SensorDataQoS().keep_last(1);
    state_subscription_ = create_subscription<sensor_msgs::msg::JointState>(
      "/arm/joint_states", state_qos,
      std::bind(&OperationalSpaceController::on_joint_state, this, std::placeholders::_1));

    // 제어 명령은 stale queue가 위험하므로 best-effort depth 1로 최신 토크만 유지한다.
    const auto command_qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    torque_publisher_ = create_publisher<std_msgs::msg::Float64MultiArray>(
      "/arm/torque_command", command_qos);

    // 감사 통계는 손실 없이 낮은 주기(50 Hz)로 보고하도록 reliable depth 10을 사용한다.
    stats_publisher_ = create_publisher<ControlStats>(
      "/arm/control_stats", rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    // 동적 vector인 Float64MultiArray의 저장공간은 초기화 시 3칸을 확보해 콜백의 resize를 피한다.
    torque_message_.data.resize(3);
    stats_message_.header.frame_id = "base_link";

    RCLCPP_INFO(
      get_logger(),
      "Operational-space controller ready: fixed 3x3/2x2 kernel, 10:1 stats decimation");
  }

private:
  // JointState의 name 배열을 기준으로 원하는 관절 값을 찾는다.
  // 메시지 배열 순서를 무조건 믿지 않는 것이 서로 다른 driver를 연결할 때 중요한 ROS 계약이다.
  static bool extract_joint_state(
    const sensor_msgs::msg::JointState & message,
    study::Vec3 & q,
    study::Vec3 & q_dot)
  {
    constexpr std::array<const char *, 3> kExpectedNames{
      "joint_1", "joint_2", "joint_3"};
    if (message.position.size() != message.name.size() ||
      message.velocity.size() != message.name.size())
    {
      return false;
    }

    for (std::size_t expected = 0; expected < 3; ++expected) {
      bool found = false;
      for (std::size_t index = 0; index < message.name.size(); ++index) {
        if (message.name[index] == kExpectedNames[expected]) {
          q[expected] = message.position[index];
          q_dot[expected] = message.velocity[index];
          found = std::isfinite(q[expected]) && std::isfinite(q_dot[expected]);
          break;
        }
      }
      if (!found) {
        return false;
      }
    }
    return true;
  }

  // 말단 원 궤적의 위치/속도/가속도를 해석식으로 만든다.
  // 미분을 수치 차분하지 않으므로 샘플 노이즈가 제어 가속도에 증폭되지 않는다.
  static void desired_trajectory(
    const double time_seconds,
    study::Vec2 & position,
    study::Vec2 & velocity,
    study::Vec2 & acceleration)
  {
    constexpr double kCenterX = 1.18;
    constexpr double kCenterY = 0.14;
    constexpr double kRadiusM = 0.08;
    constexpr double kAngularRate = 0.42;
    const double phase = kAngularRate * time_seconds;

    position = study::Vec2{
      kCenterX + kRadiusM * std::cos(phase),
      kCenterY + kRadiusM * std::sin(phase)};
    velocity = study::Vec2{
      -kRadiusM * kAngularRate * std::sin(phase),
      kRadiusM * kAngularRate * std::cos(phase)};
    acceleration = study::Vec2{
      -kRadiusM * kAngularRate * kAngularRate * std::cos(phase),
      -kRadiusM * kAngularRate * kAngularRate * std::sin(phase)};
  }

  // JointState 한 샘플을 작업공간 토크로 변환한다. DDS 직렬화 경계 바깥의 핵심 수치는 모두 고정 배열이다.
  void on_joint_state(const sensor_msgs::msg::JointState::SharedPtr message)
  {
    const auto callback_start = std::chrono::steady_clock::now();
    study::Vec3 q{};
    study::Vec3 q_dot{};
    if (!extract_joint_state(*message, q, q_dot)) {
      ++rejected_states_;
      return;
    }

    const auto steady_now = std::chrono::steady_clock::now();
    const double time_seconds =
      std::chrono::duration<double>(steady_now - start_time_).count();

    const study::Vec2 actual_position = study::forward_kinematics(q);
    const study::Mat23 jacobian = study::jacobian(q);
    const study::Vec2 jdot_qdot = study::jacobian_dot_times_velocity(q, q_dot);
    const study::Mat3 mass = study::mass_matrix(q);
    const study::Vec3 bias = study::dynamics_bias(q, q_dot);

    study::Mat3 mass_inverse{};
    double mass_determinant = 0.0;
    if (!study::inverse3(mass, mass_inverse, mass_determinant)) {
      publish_safe_bias_command(bias);
      ++numerical_failures_;
      return;
    }

    // B = M^-1 J^T. 3x2 고정 행렬이며 동적 일관 pseudoinverse의 공통 항이다.
    study::Mat32 mass_inverse_jacobian_transpose{};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t axis = 0; axis < 2; ++axis) {
        for (std::size_t col = 0; col < 3; ++col) {
          mass_inverse_jacobian_transpose[row][axis] +=
            mass_inverse[row][col] * jacobian[axis][col];
        }
      }
    }

    // A = J M^-1 J^T는 작업공간 inverse inertia이고 Lambda = A^-1가 작업공간 관성이다.
    study::Mat2 task_inverse_inertia{};
    for (std::size_t row = 0; row < 2; ++row) {
      for (std::size_t col = 0; col < 2; ++col) {
        for (std::size_t joint = 0; joint < 3; ++joint) {
          task_inverse_inertia[row][col] +=
            jacobian[row][joint] * mass_inverse_jacobian_transpose[joint][col];
        }
      }
    }
    const double raw_task_determinant =
      task_inverse_inertia[0][0] * task_inverse_inertia[1][1] -
      task_inverse_inertia[0][1] * task_inverse_inertia[1][0];

    // det(A)가 작으면 두 작업축 중 한 방향을 움직이기 어려운 특이 자세에 가깝다.
    // A + lambda^2 I로 감쇠해 역행렬 폭주를 막되, guard 활성 사실을 통계에 남긴다.
    const bool singularity_guard_active = raw_task_determinant < 0.010;
    const double damping_lambda = singularity_guard_active ? 0.040 : 0.0001;
    study::Mat2 regularized = task_inverse_inertia;
    regularized[0][0] += damping_lambda * damping_lambda;
    regularized[1][1] += damping_lambda * damping_lambda;

    study::Mat2 task_inertia{};
    double regularized_determinant = 0.0;
    if (!study::inverse2(regularized, task_inertia, regularized_determinant)) {
      publish_safe_bias_command(bias);
      ++numerical_failures_;
      return;
    }

    // J_bar = M^-1 J^T Lambda는 동적 일관 generalized inverse이다.
    // 일반 J^+와 달리 관절 질량을 고려하므로 null-space 토크가 주 작업 가속도에 덜 새어든다.
    study::Mat32 dynamically_consistent_inverse{};
    for (std::size_t joint = 0; joint < 3; ++joint) {
      for (std::size_t axis = 0; axis < 2; ++axis) {
        for (std::size_t inner = 0; inner < 2; ++inner) {
          dynamically_consistent_inverse[joint][axis] +=
            mass_inverse_jacobian_transpose[joint][inner] * task_inertia[inner][axis];
        }
      }
    }

    study::Vec2 desired_position{};
    study::Vec2 desired_velocity{};
    study::Vec2 desired_acceleration{};
    desired_trajectory(
      time_seconds, desired_position, desired_velocity, desired_acceleration);

    study::Vec2 actual_velocity{};
    for (std::size_t axis = 0; axis < 2; ++axis) {
      for (std::size_t joint = 0; joint < 3; ++joint) {
        actual_velocity[axis] += jacobian[axis][joint] * q_dot[joint];
      }
    }

    // a_cmd = x_ddot_d + Kd(x_dot_d-x_dot) + Kp(x_d-x) - J_dot q_dot.
    // 마지막 항을 빼면 x_ddot = J q_ddot + J_dot q_dot의 기구학 bias가 보상된다.
    constexpr double kTaskKp = 85.0;
    constexpr double kTaskKd = 19.0;
    study::Vec2 commanded_task_acceleration{};
    for (std::size_t axis = 0; axis < 2; ++axis) {
      commanded_task_acceleration[axis] = desired_acceleration[axis] +
        kTaskKd * (desired_velocity[axis] - actual_velocity[axis]) +
        kTaskKp * (desired_position[axis] - actual_position[axis]) -
        jdot_qdot[axis];
    }

    // F = Lambda a_cmd, tau_task = J^T F가 operational-space 주 작업 토크다.
    study::Vec2 task_force{};
    for (std::size_t row = 0; row < 2; ++row) {
      for (std::size_t col = 0; col < 2; ++col) {
        task_force[row] += task_inertia[row][col] * commanded_task_acceleration[col];
      }
    }
    study::Vec3 task_torque{};
    for (std::size_t joint = 0; joint < 3; ++joint) {
      for (std::size_t axis = 0; axis < 2; ++axis) {
        task_torque[joint] += jacobian[axis][joint] * task_force[axis];
      }
    }

    // tau_0는 남는 자유도로 home posture를 향하게 하는 저우선순위 PD 토크다.
    constexpr study::Vec3 kHomePosture{0.35, -0.75, 0.55};
    constexpr double kPostureKp = 8.0;
    constexpr double kPostureKd = 3.5;
    study::Vec3 posture_torque{};
    for (std::size_t joint = 0; joint < 3; ++joint) {
      posture_torque[joint] =
        kPostureKp * (kHomePosture[joint] - q[joint]) - kPostureKd * q_dot[joint];
    }

    // N^T = I - J^T J_bar^T. tau_null = N^T tau_0이면 이상적인 비감쇠 경우
    // J M^-1 tau_null = 0이므로 2D 말단 가속도를 바꾸지 않고 자세만 조절한다.
    study::Mat3 nullspace_transpose{};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t col = 0; col < 3; ++col) {
        nullspace_transpose[row][col] = row == col ? 1.0 : 0.0;
        for (std::size_t axis = 0; axis < 2; ++axis) {
          nullspace_transpose[row][col] -=
            jacobian[axis][row] * dynamically_consistent_inverse[col][axis];
        }
      }
    }
    study::Vec3 nullspace_torque{};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t col = 0; col < 3; ++col) {
        nullspace_torque[row] += nullspace_transpose[row][col] * posture_torque[col];
      }
    }

    // 수식상 leakage = ||J M^-1 tau_null||. 0에 가까울수록 보조 토크가 주 작업을 덜 방해한다.
    study::Vec3 nullspace_acceleration{};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t col = 0; col < 3; ++col) {
        nullspace_acceleration[row] += mass_inverse[row][col] * nullspace_torque[col];
      }
    }
    study::Vec2 task_leakage_vector{};
    for (std::size_t axis = 0; axis < 2; ++axis) {
      for (std::size_t joint = 0; joint < 3; ++joint) {
        task_leakage_vector[axis] += jacobian[axis][joint] * nullspace_acceleration[joint];
      }
    }
    const double nullspace_leakage = std::hypot(
      task_leakage_vector[0], task_leakage_vector[1]);

    // 최종 tau = bias + tau_task + tau_null. bias는 중력/점성 항을 상쇄한다.
    constexpr double kTorqueLimitNm = 60.0;
    study::Vec3 command{};
    double squared_torque_norm = 0.0;
    for (std::size_t joint = 0; joint < 3; ++joint) {
      const double raw = bias[joint] + task_torque[joint] + nullspace_torque[joint];
      command[joint] = std::clamp(raw, -kTorqueLimitNm, kTorqueLimitNm);
      squared_torque_norm += command[joint] * command[joint];
      torque_message_.data[joint] = command[joint];
    }
    torque_publisher_->publish(torque_message_);

    ++sequence_;
    const auto callback_end = std::chrono::steady_clock::now();
    const double callback_microseconds =
      std::chrono::duration<double, std::micro>(callback_end - callback_start).count();

    // 매 10번째 상태만 통계로 내보내 500 Hz 제어와 50 Hz 관측 경계를 분리한다.
    if (sequence_ % 10U == 0U) {
      stats_message_.header.stamp = message->header.stamp;
      stats_message_.sequence = sequence_;
      stats_message_.desired_xy_m[0] = desired_position[0];
      stats_message_.desired_xy_m[1] = desired_position[1];
      stats_message_.actual_xy_m[0] = actual_position[0];
      stats_message_.actual_xy_m[1] = actual_position[1];
      stats_message_.task_error_m = std::hypot(
        desired_position[0] - actual_position[0],
        desired_position[1] - actual_position[1]);
      stats_message_.nullspace_leakage = nullspace_leakage;
      stats_message_.task_inertia_det = raw_task_determinant;
      stats_message_.damping_lambda = damping_lambda;
      stats_message_.torque_norm_nm = std::sqrt(squared_torque_norm);
      stats_message_.callback_us = callback_microseconds;
      stats_message_.singularity_guard_active = singularity_guard_active;
      stats_publisher_->publish(stats_message_);
    }
  }

  // 역행렬 실패 시 작업 토크를 포기하고 중력/점성 보상만 보내는 유한한 fallback이다.
  void publish_safe_bias_command(const study::Vec3 & bias)
  {
    for (std::size_t joint = 0; joint < 3; ++joint) {
      torque_message_.data[joint] = std::clamp(bias[joint], -60.0, 60.0);
    }
    torque_publisher_->publish(torque_message_);
  }

  std::chrono::steady_clock::time_point start_time_;
  std::uint64_t sequence_{0};
  std::uint64_t rejected_states_{0};
  std::uint64_t numerical_failures_{0};
  std_msgs::msg::Float64MultiArray torque_message_;
  ControlStats stats_message_;

  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr state_subscription_;
  rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr torque_publisher_;
  rclcpp::Publisher<ControlStats>::SharedPtr stats_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // 기본 spin은 하나의 executor thread에서 콜백을 직렬 실행한다.
  // 수치 커널의 고정 연산은 보여 주지만, 이 실행만으로 hard-RT/WCET가 보장되지는 않는다.
  rclcpp::spin(std::make_shared<OperationalSpaceController>());
  rclcpp::shutdown();
  return 0;
}
