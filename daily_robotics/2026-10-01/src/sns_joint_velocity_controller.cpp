#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>

#include "geometry_msgs/msg/twist_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

#include "daily_robotics_2026_10_01/msg/joint_velocity_command.hpp"
#include "daily_robotics_2026_10_01/msg/sns_status.hpp"

using JointVelocityCommand = daily_robotics_2026_10_01::msg::JointVelocityCommand;
using SnsStatus = daily_robotics_2026_10_01::msg::SnsStatus;

namespace
{
using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;
using Mat23 = std::array<Vec3, 2>;

constexpr Vec3 kVelocityLimitRadS{0.35, 0.25, 0.45};
constexpr double kLink1M = 0.70;
constexpr double kLink2M = 0.55;
constexpr double kLink3M = 0.35;

// J(q)는 q_dot을 말단 평면 속도 x_dot으로 바꾸는 2x3 Jacobian이다.
// 수식 x_dot = J(q) q_dot의 각 열은 해당 관절을 1 rad/s 움직였을 때 말단 속도다.
Mat23 compute_jacobian(const Vec3 & q)
{
  const double q12 = q[0] + q[1];
  const double q123 = q12 + q[2];
  const double s1 = std::sin(q[0]);
  const double c1 = std::cos(q[0]);
  const double s12 = std::sin(q12);
  const double c12 = std::cos(q12);
  const double s123 = std::sin(q123);
  const double c123 = std::cos(q123);

  return Mat23{
    Vec3{
      -kLink1M * s1 - kLink2M * s12 - kLink3M * s123,
      -kLink2M * s12 - kLink3M * s123,
      -kLink3M * s123},
    Vec3{
      kLink1M * c1 + kLink2M * c12 + kLink3M * c123,
      kLink2M * c12 + kLink3M * c123,
      kLink3M * c123}}
  ;
}

Vec2 multiply(const Mat23 & jacobian, const Vec3 & vector)
{
  Vec2 result{};
  for (std::size_t axis = 0; axis < result.size(); ++axis) {
    for (std::size_t joint = 0; joint < vector.size(); ++joint) {
      result[axis] += jacobian[axis][joint] * vector[joint];
    }
  }
  return result;
}

// cond(J)=sigma_max/sigma_min이다. J*J^T의 고유값이 sigma^2이므로 2x2 닫힌식으로 계산한다.
// 값이 클수록 어떤 Cartesian 방향을 만들기 위해 큰 관절 속도가 필요한 특이 자세에 가깝다.
double condition_number(const Mat23 & jacobian)
{
  double a00 = 0.0;
  double a01 = 0.0;
  double a11 = 0.0;
  for (std::size_t joint = 0; joint < 3; ++joint) {
    a00 += jacobian[0][joint] * jacobian[0][joint];
    a01 += jacobian[0][joint] * jacobian[1][joint];
    a11 += jacobian[1][joint] * jacobian[1][joint];
  }
  const double trace = a00 + a11;
  const double discriminant = std::sqrt(std::max(
      0.0, (a00 - a11) * (a00 - a11) + 4.0 * a01 * a01));
  const double lambda_max = 0.5 * (trace + discriminant);
  const double lambda_min = 0.5 * (trace - discriminant);
  if (lambda_min <= 1.0e-12 || !std::isfinite(lambda_min)) {
    return 1.0e6;
  }
  return std::sqrt(lambda_max / lambda_min);
}

struct AffineSolution
{
  // 주어진 active set에서 q_dot(s) = a*s + b가 된다.
  Vec3 a{};
  Vec3 b{};
  bool valid{false};
};

// free[j]=true인 관절만 쓰는 weighted pseudoinverse를 2x2 역행렬로 계산한다.
// q_dot = q_N + W J^T (J W J^T + lambda^2 I)^-1 (s*x_dot - J*q_N)
// 를 s에 대해 정리하면 q_dot(s)=a*s+b이고, 이후 모든 한계를 1차 구간 교차로 검사할 수 있다.
AffineSolution affine_solution(
  const Mat23 & jacobian,
  const Vec2 & desired,
  const std::array<bool, 3> & free,
  const Vec3 & fixed,
  const double damping)
{
  double gram00 = 0.0;
  double gram01 = 0.0;
  double gram11 = 0.0;
  for (std::size_t joint = 0; joint < 3; ++joint) {
    if (!free[joint]) {
      continue;
    }
    gram00 += jacobian[0][joint] * jacobian[0][joint];
    gram01 += jacobian[0][joint] * jacobian[1][joint];
    gram11 += jacobian[1][joint] * jacobian[1][joint];
  }

  // 감쇠 전 최소 고유값으로 남은 자유 관절들이 2D 작업 rank를 유지하는지 확인한다.
  const double trace = gram00 + gram11;
  const double discriminant = std::sqrt(std::max(
      0.0, (gram00 - gram11) * (gram00 - gram11) + 4.0 * gram01 * gram01));
  const double minimum_eigenvalue = 0.5 * (trace - discriminant);
  if (minimum_eigenvalue <= 1.0e-10) {
    return AffineSolution{};
  }

  gram00 += damping * damping;
  gram11 += damping * damping;
  const double determinant = gram00 * gram11 - gram01 * gram01;
  if (std::abs(determinant) <= 1.0e-15 || !std::isfinite(determinant)) {
    return AffineSolution{};
  }

  // [[a,b],[b,d]]^-1 = 1/(ad-b^2) [[d,-b],[-b,a]].
  const double inv00 = gram11 / determinant;
  const double inv01 = -gram01 / determinant;
  const double inv11 = gram00 / determinant;

  const Vec2 projected_fixed = multiply(jacobian, fixed);
  const Vec2 rhs_b{-projected_fixed[0], -projected_fixed[1]};
  const Vec2 inverse_desired{
    inv00 * desired[0] + inv01 * desired[1],
    inv01 * desired[0] + inv11 * desired[1]};
  const Vec2 inverse_b{
    inv00 * rhs_b[0] + inv01 * rhs_b[1],
    inv01 * rhs_b[0] + inv11 * rhs_b[1]};

  AffineSolution solution;
  solution.b = fixed;
  for (std::size_t joint = 0; joint < 3; ++joint) {
    if (free[joint]) {
      solution.a[joint] =
        jacobian[0][joint] * inverse_desired[0] +
        jacobian[1][joint] * inverse_desired[1];
      solution.b[joint] +=
        jacobian[0][joint] * inverse_b[0] +
        jacobian[1][joint] * inverse_b[1];
    }
  }
  solution.valid = true;
  return solution;
}

// 모든 관절 부등식 -limit <= a*s+b <= limit의 교집합에서 가장 큰 s를 찾는다.
// s가 1이면 원래 작업을 달성하고, 1보다 작으면 방향은 유지한 채 속도 크기만 줄인다.
double maximum_feasible_scale(
  const AffineSolution & solution,
  const std::array<bool, 3> & free)
{
  double lower_scale = 0.0;
  double upper_scale = 1.0;
  for (std::size_t joint = 0; joint < 3; ++joint) {
    if (!free[joint]) {
      continue;
    }
    const double coefficient = solution.a[joint];
    const double offset = solution.b[joint];
    if (std::abs(coefficient) < 1.0e-12) {
      if (std::abs(offset) > kVelocityLimitRadS[joint] + 1.0e-12) {
        return -1.0;
      }
      continue;
    }
    const double first = (-kVelocityLimitRadS[joint] - offset) / coefficient;
    const double second = (kVelocityLimitRadS[joint] - offset) / coefficient;
    lower_scale = std::max(lower_scale, std::min(first, second));
    upper_scale = std::min(upper_scale, std::max(first, second));
  }
  if (lower_scale > upper_scale || upper_scale < 0.0 || lower_scale > 1.0) {
    return -1.0;
  }
  return std::clamp(upper_scale, 0.0, 1.0);
}

struct SnsResult
{
  Vec3 velocity{};
  double task_scale{0.0};
  std::uint8_t saturated_mask{0};
  bool numerical_fallback{false};
};

// 최대 3개 관절만 차례로 포화시키는 bounded SNS teaching kernel이다.
// 각 반복은 "현재 active set의 최선 scale 저장 → s=1 위반이 가장 큰 관절 포화" 순서다.
SnsResult solve_sns(
  const Mat23 & jacobian,
  const Vec2 & desired,
  const double damping)
{
  std::array<bool, 3> free{true, true, true};
  Vec3 fixed{};
  Vec3 best_velocity{};
  double best_scale = -1.0;
  std::uint8_t best_active_mask = 0;
  bool found_valid_solution = false;

  for (std::size_t iteration = 0; iteration < 3; ++iteration) {
    const AffineSolution affine = affine_solution(jacobian, desired, free, fixed, damping);
    if (!affine.valid) {
      break;
    }

    const double feasible_scale = maximum_feasible_scale(affine, free);
    if (feasible_scale >= 0.0 && feasible_scale > best_scale + 1.0e-12) {
      best_scale = feasible_scale;
      for (std::size_t joint = 0; joint < 3; ++joint) {
        best_velocity[joint] = affine.a[joint] * feasible_scale + affine.b[joint];
      }
      found_valid_solution = true;
      best_active_mask = 0;
      for (std::size_t joint = 0; joint < 3; ++joint) {
        if (!free[joint]) {
          best_active_mask = static_cast<std::uint8_t>(
            best_active_mask | static_cast<std::uint8_t>(1U << joint));
        }
      }
    }

    // s=1 후보를 만들어 가장 심하게 정규화 한계를 넘는 관절을 찾는다.
    std::size_t worst_joint = 3;
    double worst_violation_ratio = 1.0;
    Vec3 full_task_candidate{};
    for (std::size_t joint = 0; joint < 3; ++joint) {
      full_task_candidate[joint] = affine.a[joint] + affine.b[joint];
      if (free[joint]) {
        const double ratio = std::abs(full_task_candidate[joint]) / kVelocityLimitRadS[joint];
        if (ratio > worst_violation_ratio) {
          worst_violation_ratio = ratio;
          worst_joint = joint;
        }
      }
    }

    if (worst_joint == 3) {
      best_velocity = full_task_candidate;
      best_scale = 1.0;
      found_valid_solution = true;
      break;
    }

    // 가장 큰 위반 관절을 정확히 상/하한에 고정하고 다음 반복에서 남은 null space를 사용한다.
    free[worst_joint] = false;
    fixed[worst_joint] = std::copysign(
      kVelocityLimitRadS[worst_joint], full_task_candidate[worst_joint]);
  }

  SnsResult result;
  if (!found_valid_solution) {
    // 역행렬/rank 실패 시 유한한 0 명령으로 폴백한다. NaN을 actuator로 보내지 않는 마지막 안전선이다.
    result.numerical_fallback = true;
    return result;
  }

  result.task_scale = std::clamp(best_scale, 0.0, 1.0);
  result.saturated_mask = best_active_mask;
  for (std::size_t joint = 0; joint < 3; ++joint) {
    result.velocity[joint] = std::clamp(
      best_velocity[joint], -kVelocityLimitRadS[joint], kVelocityLimitRadS[joint]);
    if (std::abs(std::abs(result.velocity[joint]) - kVelocityLimitRadS[joint]) < 1.0e-6) {
      result.saturated_mask = static_cast<std::uint8_t>(
        result.saturated_mask | static_cast<std::uint8_t>(1U << joint));
    }
    if (!std::isfinite(result.velocity[joint])) {
      result.velocity.fill(0.0);
      result.task_scale = 0.0;
      result.saturated_mask = 0;
      result.numerical_fallback = true;
      break;
    }
  }
  return result;
}
}  // namespace

// 이 노드는 JointState와 Cartesian 목표를 받아 제한을 절대 넘지 않는 관절 속도 명령을 만든다.
// 로봇 시스템에서의 역할은 redundancy를 먼저 활용하고, 불가능할 때만 작업 속도를 최소한으로 축소하는 것이다.
class SnsJointVelocityController : public rclcpp::Node
{
public:
  SnsJointVelocityController()
  : Node("sns_joint_velocity_controller")
  {
    const auto input_qos = rclcpp::SensorDataQoS().keep_last(1);
    joint_state_subscription_ = create_subscription<sensor_msgs::msg::JointState>(
      "/joint_states", input_qos,
      std::bind(&SnsJointVelocityController::on_joint_state, this, std::placeholders::_1));
    target_subscription_ = create_subscription<geometry_msgs::msg::TwistStamped>(
      "/cartesian_velocity_target", input_qos,
      std::bind(&SnsJointVelocityController::on_target, this, std::placeholders::_1));

    command_publisher_ = create_publisher<JointVelocityCommand>(
      "/joint_velocity_command", rclcpp::QoS(rclcpp::KeepLast(1)).best_effort());
    // 진단은 감사 데이터이므로 reliable depth 10으로 손실 가능성을 낮추고 50 Hz로만 발행한다.
    status_publisher_ = create_publisher<SnsStatus>(
      "/sns/status", rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    // 2 ms wall timer는 500 Hz 제어 주기를 만든다. 일반 Linux에서 관측한 주기이지 hard-RT 보장은 아니다.
    control_timer_ = create_wall_timer(
      std::chrono::milliseconds(2),
      std::bind(&SnsJointVelocityController::on_control_tick, this));

    RCLCPP_INFO(
      get_logger(), "SNS controller ready: 500 Hz, <=3 active-set rounds, limits=[.35 .25 .45]");
  }

private:
  // JointState 배열 순서를 믿지 않고 name으로 세 관절을 찾는다.
  // driver마다 관절 순서가 달라도 이름 계약만 맞으면 잘못된 축에 명령하지 않는다.
  void on_joint_state(const sensor_msgs::msg::JointState::SharedPtr message)
  {
    constexpr std::array<const char *, 3> kNames{"joint_1", "joint_2", "joint_3"};
    if (message->name.size() != message->position.size()) {
      return;
    }
    Vec3 extracted{};
    for (std::size_t expected = 0; expected < kNames.size(); ++expected) {
      bool found = false;
      for (std::size_t index = 0; index < message->name.size(); ++index) {
        if (message->name[index] == kNames[expected]) {
          extracted[expected] = message->position[index];
          found = std::isfinite(extracted[expected]);
          break;
        }
      }
      if (!found) {
        return;
      }
    }
    joint_position_rad_ = extracted;
    state_received_ = true;
    last_state_receipt_ = std::chrono::steady_clock::now();
  }

  // TwistStamped의 frame_id는 속도가 어느 좌표계 표현인지 결정한다.
  // 이 예제는 base_link의 x/y 선속도만 지원하므로 다른 frame 또는 NaN은 거부한다.
  void on_target(const geometry_msgs::msg::TwistStamped::SharedPtr message)
  {
    if (message->header.frame_id != "base_link" ||
      !std::isfinite(message->twist.linear.x) || !std::isfinite(message->twist.linear.y))
    {
      return;
    }
    desired_velocity_m_s_ = {message->twist.linear.x, message->twist.linear.y};
    target_received_ = true;
    last_target_receipt_ = std::chrono::steady_clock::now();
  }

  // 500 Hz hot path다. ROS 메시지 경계 밖의 수치는 전부 std::array 고정 크기라 heap 할당이 없다.
  void on_control_tick()
  {
    const auto kernel_start = std::chrono::steady_clock::now();
    ++sequence_;

    const auto steady_now = std::chrono::steady_clock::now();
    const bool fresh_state = state_received_ &&
      (steady_now - last_state_receipt_ < std::chrono::milliseconds(100));
    const bool fresh_target = target_received_ &&
      (steady_now - last_target_receipt_ < std::chrono::milliseconds(100));

    const Mat23 jacobian = compute_jacobian(joint_position_rad_);
    const double jacobian_condition = condition_number(jacobian);
    const bool singularity_guard = jacobian_condition > 20.0;
    // 특이점 근처에서는 DLS의 lambda를 키워 작은 sigma 방향의 관절 속도 폭주를 억제한다.
    const double damping = singularity_guard ? 1.0e-4 : 1.0e-6;

    SnsResult result;
    if (fresh_state && fresh_target) {
      result = solve_sns(jacobian, desired_velocity_m_s_, damping);
    } else {
      result.numerical_fallback = true;
    }

    const Vec2 achieved = multiply(jacobian, result.velocity);
    const double residual_x = achieved[0] - result.task_scale * desired_velocity_m_s_[0];
    const double residual_y = achieved[1] - result.task_scale * desired_velocity_m_s_[1];
    const double residual = std::hypot(residual_x, residual_y);

    const auto command_stamp = now();
    command_message_.header.stamp = command_stamp;
    command_message_.sequence = sequence_;
    command_message_.velocity_rad_s = result.velocity;
    command_publisher_->publish(command_message_);

    const auto kernel_end = std::chrono::steady_clock::now();
    const double kernel_microseconds = std::chrono::duration<double, std::micro>(
      kernel_end - kernel_start).count();

    // 진단 serialization 부하를 낮추기 위해 매 10번째 제어 tick, 즉 50 Hz로만 상태를 보낸다.
    if ((sequence_ % 10U) == 0U) {
      status_message_.header.stamp = command_stamp;
      status_message_.header.frame_id = "base_link";
      status_message_.sequence = sequence_;
      status_message_.joint_position_rad = joint_position_rad_;
      status_message_.desired_xy_m_s = desired_velocity_m_s_;
      status_message_.achieved_xy_m_s = achieved;
      status_message_.task_scale = result.task_scale;
      status_message_.scaled_residual_m_s = residual;
      status_message_.condition_number = jacobian_condition;
      status_message_.callback_us = kernel_microseconds;
      status_message_.saturated_mask = result.saturated_mask;
      status_message_.singularity_guard_active = singularity_guard;
      status_message_.numerical_fallback_active = result.numerical_fallback;
      status_publisher_->publish(status_message_);
    }
  }

  Vec3 joint_position_rad_{};
  Vec2 desired_velocity_m_s_{};
  bool state_received_{false};
  bool target_received_{false};
  std::chrono::steady_clock::time_point last_state_receipt_{};
  std::chrono::steady_clock::time_point last_target_receipt_{};
  std::uint64_t sequence_{0};
  JointVelocityCommand command_message_;
  SnsStatus status_message_;

  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_state_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr target_subscription_;
  rclcpp::Publisher<JointVelocityCommand>::SharedPtr command_publisher_;
  rclcpp::Publisher<SnsStatus>::SharedPtr status_publisher_;
  rclcpp::TimerBase::SharedPtr control_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // 단일 thread executor를 사용해 상태 갱신과 500 Hz hot path의 동시 접근을 없앤다.
  // 이 선택은 race를 단순화할 뿐 Linux scheduling latency나 WCET를 보장하지는 않는다.
  rclcpp::spin(std::make_shared<SnsJointVelocityController>());
  rclcpp::shutdown();
  return 0;
}
