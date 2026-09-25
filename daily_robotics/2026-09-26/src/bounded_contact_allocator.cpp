#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include "daily_robotics_2026_09_26/msg/contact_solution.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

namespace daily_robotics
{

using ContactSolution = daily_robotics_2026_09_26::msg::ContactSolution;

// 단일 producer와 단일 consumer만 사용하는 고정 용량 ring buffer다.
// ROS callback과 계산 스레드 사이에서 mutex 대기와 실행 중 동적 할당을 피하는 것이 목적이다.
template<typename T, std::size_t Capacity>
class SpscQueue
{
  static_assert(Capacity >= 2U, "ring buffer needs one payload slot and one sentinel slot");

public:
  bool try_push(const T & value) noexcept
  {
    // producer만 head를 쓰므로 relaxed load면 충분하다.
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = (head + 1U) % Capacity;
    // acquire는 consumer가 tail을 release한 뒤 해당 슬롯 재사용이 안전함을 보장한다.
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    storage_[head] = value;
    // 슬롯 기록이 끝난 다음 head를 공개해야 consumer가 반쯤 쓴 값을 읽지 않는다.
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool try_pop(T & value) noexcept
  {
    // consumer만 tail을 쓰므로 자신의 인덱스는 relaxed로 읽는다.
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    // producer의 release head와 짝을 이루어 완성된 payload만 관측한다.
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    value = storage_[tail];
    tail_.store((tail + 1U) % Capacity, std::memory_order_release);
    return true;
  }

  bool indices_are_lock_free() const noexcept
  {
    // lock-free 여부는 타입 이름만 보고 단정하지 않고 실제 플랫폼에서 확인한다.
    return head_.is_lock_free() && tail_.is_lock_free();
  }

private:
  std::array<T, Capacity> storage_{};
  alignas(64) std::atomic<std::size_t> head_{0U};
  alignas(64) std::atomic<std::size_t> tail_{0U};
};

struct WrenchCommand
{
  std::uint64_t sequence{0U};
  std::array<double, 3> desired{{0.0, 196.2, 0.0}};
};

struct SolutionRecord
{
  std::uint64_t input_sequence{0U};
  std::array<double, 3> desired{{0.0, 196.2, 0.0}};
  // [left_fx, left_fz, right_fx, right_fz] 순서는 행렬 A의 열 순서와 같다.
  std::array<double, 4> forces{{0.0, 98.1, 0.0, 98.1}};
  std::array<double, 3> achieved{{0.0, 196.2, 0.0}};
  std::uint32_t iterations{0U};
  double objective{0.0};
  double wrench_residual{0.0};
  double projected_gradient_norm{0.0};
  double max_constraint_violation{0.0};
  double solve_time_us{0.0};
  bool converged{false};
  bool feasible{false};
};

// 이 노드는 목표 centroidal wrench를 두 발의 접촉력으로 분배한다.
// ROS 통신 callback은 비실시간 경로에 두고, 200 Hz 계산 커널은 별도 스레드에서 고정 배열과
// 고정 반복 횟수만 사용하여 데이터 의존적인 실행량과 heap 할당을 피한다.
class BoundedContactAllocator final : public rclcpp::Node
{
public:
  BoundedContactAllocator()
  : Node("bounded_contact_allocator")
  {
    friction_coefficient_ = declare_parameter<double>("friction_coefficient", 0.6);
    min_normal_force_ = declare_parameter<double>("min_normal_force", 15.0);
    max_normal_force_ = declare_parameter<double>("max_normal_force", 180.0);
    contact_half_span_ = declare_parameter<double>("contact_half_span", 0.25);
    com_height_ = declare_parameter<double>("com_height", 0.55);
    smooth_weight_ = declare_parameter<double>("smooth_weight", 0.02);
    kernel_frequency_hz_ = declare_parameter<double>("kernel_frequency_hz", 200.0);
    const int requested_iterations = declare_parameter<int>("solver_iterations", 64);

    // 파라미터가 잘못되어도 RT loop의 상한이 무너지지 않도록 시작 시 한 번 범위를 고정한다.
    solver_iterations_ = static_cast<std::uint32_t>(std::clamp(requested_iterations, 8, 128));
    if (!(friction_coefficient_ > 0.0) || !(min_normal_force_ >= 0.0) ||
      !(max_normal_force_ > min_normal_force_) || !(contact_half_span_ > 0.0) ||
      !(com_height_ > 0.0) || !(kernel_frequency_hz_ >= 50.0))
    {
      throw std::invalid_argument("contact geometry, force limits, or kernel frequency is invalid");
    }

    // KeepLast(1)은 오래된 상위 명령보다 최신 wrench를 우선한다. callback에서는 queue에 복사만 한다.
    const auto command_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
    command_subscription_ = create_subscription<geometry_msgs::msg::WrenchStamped>(
      "/wbc/desired_wrench", command_qos,
      [this](geometry_msgs::msg::WrenchStamped::ConstSharedPtr message) {
        receive_command(*message);
      });

    // 해와 접촉력은 관찰/감사용 상태이므로 최근 샘플을 보존하는 reliable QoS를 쓴다.
    const auto state_qos = rclcpp::QoS(rclcpp::KeepLast(5)).reliable();
    solution_publisher_ = create_publisher<ContactSolution>(
      "/wbc/contact_solution", state_qos);
    left_wrench_publisher_ = create_publisher<geometry_msgs::msg::WrenchStamped>(
      "/wbc/left_contact_wrench", state_qos);
    right_wrench_publisher_ = create_publisher<geometry_msgs::msg::WrenchStamped>(
      "/wbc/right_contact_wrench", state_qos);

    // 일반 ROS publish는 DDS 내부에서 lock/할당할 수 있으므로 RT 커널이 직접 호출하지 않는다.
    // 이 50 Hz timer가 RT→non-RT queue를 비우고 메시지로 변환한다.
    using namespace std::chrono_literals;
    publish_timer_ = create_wall_timer(20ms, [this]() {publish_latest_solution();});

    running_.store(true, std::memory_order_release);
    kernel_thread_ = std::thread([this]() {run_kernel();});

    RCLCPP_INFO(
      get_logger(),
      "bounded QP started: %.0f Hz, %u iterations, SPSC indices lock-free=%s",
      kernel_frequency_hz_, solver_iterations_,
      command_queue_.indices_are_lock_free() && solution_queue_.indices_are_lock_free() ? "true" : "false");
  }

  ~BoundedContactAllocator() override
  {
    // release store로 종료 플래그를 공개하고 join하여 Node 자원보다 계산 스레드가 먼저 끝나게 한다.
    running_.store(false, std::memory_order_release);
    if (kernel_thread_.joinable()) {
      kernel_thread_.join();
    }
  }

private:
  void receive_command(const geometry_msgs::msg::WrenchStamped & message)
  {
    WrenchCommand command;
    command.sequence = ++input_sequence_;
    command.desired = {{
      message.wrench.force.x,
      message.wrench.force.z,
      message.wrench.torque.y}};

    // queue가 가득 차면 callback을 막지 않고 새 명령을 버린다. 횟수는 진단 메시지에 공개한다.
    if (!command_queue_.try_push(command)) {
      dropped_commands_.fetch_add(1U, std::memory_order_relaxed);
    }
  }

  // 점 (tangent, normal)을 잘린 2D Coulomb cone에 정확히 Euclidean projection한다.
  // 허용 집합은 n_min <= n <= n_max, |t| <= mu*n인 볼록 사다리꼴이다.
  std::pair<double, double> project_contact(double tangent, double normal) const noexcept
  {
    const double mu = friction_coefficient_;
    if (normal >= min_normal_force_ && normal <= max_normal_force_ &&
      std::abs(tangent) <= mu * normal)
    {
      return {tangent, normal};
    }

    double best_t = 0.0;
    double best_n = min_normal_force_;
    double best_distance_sq = std::numeric_limits<double>::infinity();

    const auto consider = [&](double candidate_t, double candidate_n) {
        const double dt = candidate_t - tangent;
        const double dn = candidate_n - normal;
        const double distance_sq = dt * dt + dn * dn;
        if (distance_sq < best_distance_sq) {
          best_distance_sq = distance_sq;
          best_t = candidate_t;
          best_n = candidate_n;
        }
      };

    // 아래/위 수평 변의 선분 projection을 후보로 검사한다.
    consider(
      std::clamp(tangent, -mu * min_normal_force_, mu * min_normal_force_),
      min_normal_force_);
    consider(
      std::clamp(tangent, -mu * max_normal_force_, mu * max_normal_force_),
      max_normal_force_);

    // 두 마찰 경계 t = +/- mu*n의 선분 projection도 검사한다.
    for (const double sign : {-1.0, 1.0}) {
      const double projected_n = std::clamp(
        (sign * mu * tangent + normal) / (mu * mu + 1.0),
        min_normal_force_, max_normal_force_);
      consider(sign * mu * projected_n, projected_n);
    }
    return {best_t, best_n};
  }

  std::array<double, 3> multiply_wrench_map(const std::array<double, 4> & force) const noexcept
  {
    // w = A f. 접점 r=(x,0,-h), 힘 F=(Fx,0,Fz)이므로
    // tau_y = (r x F)_y = r_z*Fx - r_x*Fz = -h*Fx - x*Fz다.
    return {{
      force[0] + force[2],
      force[1] + force[3],
      -com_height_ * (force[0] + force[2]) +
      contact_half_span_ * force[1] - contact_half_span_ * force[3]}};
  }

  std::array<double, 4> gradient(
    const std::array<double, 4> & force,
    const std::array<double, 4> & reference,
    const std::array<double, 3> & desired) const noexcept
  {
    const auto achieved = multiply_wrench_map(force);
    const std::array<double, 3> error{{
      achieved[0] - desired[0], achieved[1] - desired[1], achieved[2] - desired[2]}};
    // pitch 오차는 힘 오차보다 작은 수치가 되기 쉬워 weight 8로 균형을 준다.
    constexpr std::array<double, 3> weights{{1.0, 1.0, 8.0}};
    const std::array<std::array<double, 4>, 3> map{{
      {{1.0, 0.0, 1.0, 0.0}},
      {{0.0, 1.0, 0.0, 1.0}},
      {{-com_height_, contact_half_span_, -com_height_, -contact_half_span_}}}};

    std::array<double, 4> result{};
    for (std::size_t column = 0U; column < result.size(); ++column) {
      for (std::size_t row = 0U; row < error.size(); ++row) {
        // ∇(1/2 ||Af-w||²_W) = Aᵀ W (Af-w): 수식의 행렬 곱을 그대로 펼친 코드다.
        result[column] += map[row][column] * weights[row] * error[row];
      }
      // 작은 smooth 항은 이전 해에서 힘이 갑자기 뛰는 현상을 줄인다.
      result[column] += smooth_weight_ * (force[column] - reference[column]);
    }
    return result;
  }

  double lipschitz_upper_bound() const noexcept
  {
    // Hessian H=AᵀWA+lambda*I의 최대 절대 행합은 spectral norm의 안전한 상계다.
    // alpha=1/L을 쓰면 projected-gradient step이 과도하게 커지는 것을 막을 수 있다.
    constexpr std::array<double, 3> weights{{1.0, 1.0, 8.0}};
    const std::array<std::array<double, 4>, 3> map{{
      {{1.0, 0.0, 1.0, 0.0}},
      {{0.0, 1.0, 0.0, 1.0}},
      {{-com_height_, contact_half_span_, -com_height_, -contact_half_span_}}}};
    double largest_row_sum = 0.0;
    for (std::size_t row_h = 0U; row_h < 4U; ++row_h) {
      double row_sum = 0.0;
      for (std::size_t column_h = 0U; column_h < 4U; ++column_h) {
        double value = row_h == column_h ? smooth_weight_ : 0.0;
        for (std::size_t task = 0U; task < 3U; ++task) {
          value += map[task][row_h] * weights[task] * map[task][column_h];
        }
        row_sum += std::abs(value);
      }
      largest_row_sum = std::max(largest_row_sum, row_sum);
    }
    return std::max(largest_row_sum, 1.0e-9);
  }

  SolutionRecord solve(
    const WrenchCommand & command, const std::array<double, 4> & warm_start) const noexcept
  {
    const auto solve_started = std::chrono::steady_clock::now();
    SolutionRecord record;
    record.input_sequence = command.sequence;
    record.desired = command.desired;
    record.forces = warm_start;
    record.iterations = solver_iterations_;

    const std::array<double, 4> reference = warm_start;
    const double step = 1.0 / lipschitz_upper_bound();

    // 반복 횟수를 오차에 따라 조기 종료하지 않는다. 매 tick의 연산량 상한을 동일하게 유지한다.
    for (std::uint32_t iteration = 0U; iteration < solver_iterations_; ++iteration) {
      const auto grad = gradient(record.forces, reference, command.desired);
      for (std::size_t index = 0U; index < record.forces.size(); ++index) {
        record.forces[index] -= step * grad[index];
      }
      // 각 발의 [tangent, normal] 쌍을 마찰/수직력 허용 집합으로 되돌린다.
      const auto left = project_contact(record.forces[0], record.forces[1]);
      const auto right = project_contact(record.forces[2], record.forces[3]);
      record.forces = {{left.first, left.second, right.first, right.second}};
    }

    record.achieved = multiply_wrench_map(record.forces);
    const double error_fx = record.achieved[0] - command.desired[0];
    const double error_fz = record.achieved[1] - command.desired[1];
    const double error_tau = record.achieved[2] - command.desired[2];
    record.wrench_residual = std::sqrt(
      error_fx * error_fx + error_fz * error_fz + error_tau * error_tau);

    double smooth_cost = 0.0;
    for (std::size_t index = 0U; index < record.forces.size(); ++index) {
      const double delta = record.forces[index] - reference[index];
      smooth_cost += delta * delta;
    }
    record.objective = 0.5 * (
      error_fx * error_fx + error_fz * error_fz + 8.0 * error_tau * error_tau +
      smooth_weight_ * smooth_cost);

    // Projected-gradient norm은 제약 최적점에서 일반 gradient가 0이 아닐 수 있는 문제를 피한다.
    const auto final_gradient = gradient(record.forces, reference, command.desired);
    std::array<double, 4> probe = record.forces;
    for (std::size_t index = 0U; index < probe.size(); ++index) {
      probe[index] -= step * final_gradient[index];
    }
    const auto probe_left = project_contact(probe[0], probe[1]);
    const auto probe_right = project_contact(probe[2], probe[3]);
    probe = {{probe_left.first, probe_left.second, probe_right.first, probe_right.second}};
    for (std::size_t index = 0U; index < probe.size(); ++index) {
      record.projected_gradient_norm = std::max(
        record.projected_gradient_norm, std::abs(record.forces[index] - probe[index]) / step);
    }

    const auto constraint_violation = [this](double tangent, double normal) {
        return std::max({
          0.0,
          min_normal_force_ - normal,
          normal - max_normal_force_,
          std::abs(tangent) - friction_coefficient_ * normal});
      };
    record.max_constraint_violation = std::max(
      constraint_violation(record.forces[0], record.forces[1]),
      constraint_violation(record.forces[2], record.forces[3]));
    record.feasible = record.max_constraint_violation <= 1.0e-9 &&
      std::isfinite(record.objective) && std::isfinite(record.wrench_residual);
    // 64회 고정 예산에서 0.1 N 수준의 projected-gradient를 실습용 수렴 기준으로 사용한다.
    record.converged = record.projected_gradient_norm < 0.1;
    record.solve_time_us = std::chrono::duration<double, std::micro>(
      std::chrono::steady_clock::now() - solve_started).count();
    return record;
  }

  void run_kernel() noexcept
  {
    const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(1.0 / kernel_frequency_hz_));
    auto next_release = std::chrono::steady_clock::now() + period;
    WrenchCommand active_command;
    std::array<double, 4> warm_start{{0.0, 98.1, 0.0, 98.1}};

    while (running_.load(std::memory_order_acquire) && rclcpp::ok()) {
      // 도착한 명령을 모두 비우되 가장 최신 것만 다음 QP target으로 사용한다.
      WrenchCommand candidate;
      while (command_queue_.try_pop(candidate)) {
        active_command = candidate;
      }

      const SolutionRecord result = solve(active_command, warm_start);
      warm_start = result.forces;  // 이전 최적해를 다음 tick의 초기값으로 쓰는 warm start다.
      if (!solution_queue_.try_push(result)) {
        dropped_solutions_.fetch_add(1U, std::memory_order_relaxed);
      }

      // sleep_until은 매번 sleep_for하는 방식의 누적 drift를 줄이는 절대 주기 대기다.
      std::this_thread::sleep_until(next_release);
      next_release += period;
      const auto now = std::chrono::steady_clock::now();
      if (now > next_release + period) {
        // 심한 지연 뒤 밀린 tick을 폭주 실행하지 않고 다음 절대 경계로 재동기화한다.
        next_release = now + period;
      }
    }
  }

  void publish_latest_solution()
  {
    SolutionRecord latest;
    SolutionRecord candidate;
    bool received = false;
    // 200 Hz 중간 해는 버리고 50 Hz 관찰 시점의 최신 해만 DDS로 내보낸다.
    while (solution_queue_.try_pop(candidate)) {
      latest = candidate;
      received = true;
    }
    if (!received) {
      return;
    }

    ContactSolution message;
    message.header.stamp = now();
    message.header.frame_id = "base_link";
    message.input_sequence = latest.input_sequence;
    message.desired_fx = latest.desired[0];
    message.desired_fz = latest.desired[1];
    message.desired_tau_y = latest.desired[2];
    message.left_fx = latest.forces[0];
    message.left_fz = latest.forces[1];
    message.right_fx = latest.forces[2];
    message.right_fz = latest.forces[3];
    message.achieved_fx = latest.achieved[0];
    message.achieved_fz = latest.achieved[1];
    message.achieved_tau_y = latest.achieved[2];
    message.iterations = latest.iterations;
    message.objective = latest.objective;
    message.wrench_residual = latest.wrench_residual;
    message.projected_gradient_norm = latest.projected_gradient_norm;
    message.max_constraint_violation = latest.max_constraint_violation;
    message.solve_time_us = latest.solve_time_us;
    message.dropped_commands = dropped_commands_.load(std::memory_order_relaxed);
    message.dropped_solutions = dropped_solutions_.load(std::memory_order_relaxed);
    message.converged = latest.converged;
    message.feasible = latest.feasible;
    message.atomic_indices_lock_free = command_queue_.indices_are_lock_free() &&
      solution_queue_.indices_are_lock_free();
    solution_publisher_->publish(message);

    // 각 접촉력도 표준 WrenchStamped로 노출해 RViz/rosbag/기존 도구와 연결할 수 있게 한다.
    geometry_msgs::msg::WrenchStamped left;
    left.header = message.header;
    left.header.frame_id = "left_foot";
    left.wrench.force.x = latest.forces[0];
    left.wrench.force.z = latest.forces[1];
    left_wrench_publisher_->publish(left);
    geometry_msgs::msg::WrenchStamped right;
    right.header = message.header;
    right.header.frame_id = "right_foot";
    right.wrench.force.x = latest.forces[2];
    right.wrench.force.z = latest.forces[3];
    right_wrench_publisher_->publish(right);

    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 2000,
      "QP residual=%.4f, pg=%.4f, solve=%.2f us, feasible=%s",
      latest.wrench_residual, latest.projected_gradient_norm, latest.solve_time_us,
      latest.feasible ? "true" : "false");
  }

  double friction_coefficient_{0.6};
  double min_normal_force_{15.0};
  double max_normal_force_{180.0};
  double contact_half_span_{0.25};
  double com_height_{0.55};
  double smooth_weight_{0.02};
  double kernel_frequency_hz_{200.0};
  std::uint32_t solver_iterations_{64U};

  SpscQueue<WrenchCommand, 16U> command_queue_;
  SpscQueue<SolutionRecord, 64U> solution_queue_;
  std::atomic<bool> running_{false};
  std::atomic<std::uint64_t> dropped_commands_{0U};
  std::atomic<std::uint64_t> dropped_solutions_{0U};
  std::uint64_t input_sequence_{0U};
  std::thread kernel_thread_;

  rclcpp::Subscription<geometry_msgs::msg::WrenchStamped>::SharedPtr command_subscription_;
  rclcpp::Publisher<ContactSolution>::SharedPtr solution_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr left_wrench_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr right_wrench_publisher_;
  rclcpp::TimerBase::SharedPtr publish_timer_;
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor는 이 노드의 producer callback이 항상 하나임을 보장해 SPSC 계약을 지킨다.
  rclcpp::spin(std::make_shared<daily_robotics::BoundedContactAllocator>());
  rclcpp::shutdown();
  return 0;
}
