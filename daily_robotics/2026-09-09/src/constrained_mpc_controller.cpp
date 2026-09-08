#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include "rclcpp/executors/static_single_threaded_executor.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

// 이 노드는 관절 상태와 목표를 받아 미래 100 ms를 예측하고, 토크/토크 변화율 제약을
// 만족하는 첫 토크만 발행한다. 매 주기 다시 최적화하는 것이 receding-horizon MPC다.
class ConstrainedMpcController final : public rclcpp::Node
{
public:
  ConstrainedMpcController()
  : Node("constrained_mpc_controller")
  {
    // SensorDataQoS는 관절 상태가 고주기이고 최신 표본이 가장 중요하다는 의도를 표현한다.
    state_subscription_ = create_subscription<sensor_msgs::msg::JointState>(
      "/joint/state", rclcpp::SensorDataQoS(),
      std::bind(&ConstrainedMpcController::on_joint_state, this, std::placeholders::_1));

    // 목표 Publisher와 정확히 호환되도록 Reliable + Transient Local을 요청한다.
    // 이 Durability 덕분에 Controller가 늦게 시작해도 마지막 목표를 즉시 받는다.
    const auto target_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    target_subscription_ = create_subscription<std_msgs::msg::Float64>(
      "/joint/target", target_qos,
      std::bind(&ConstrainedMpcController::on_target, this, std::placeholders::_1));

    torque_publisher_ = create_publisher<std_msgs::msg::Float64>(
      "/motor/torque_raw", rclcpp::QoS(1).reliable());
    diagnostics_publisher_ = create_publisher<std_msgs::msg::String>(
      "/mpc/diagnostics", rclcpp::QoS(1).reliable());

    // 5 ms(200 Hz)마다 고정 횟수 projected-gradient 최적화를 수행한다.
    control_timer_ = create_wall_timer(5ms, std::bind(&ConstrainedMpcController::control_step, this));
  }

private:
  struct State
  {
    double position_rad;
    double velocity_rad_s;
  };

  // 센서 callback은 동적 연산 없이 최신 상태 두 개만 복사한다.
  void on_joint_state(const sensor_msgs::msg::JointState::SharedPtr message)
  {
    if (message->position.empty() || message->velocity.empty()) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "JointState 배열이 비어 있습니다.");
      return;
    }
    measured_state_ = State{message->position[0], message->velocity[0]};
    state_received_ = true;
  }

  // 상위 계획기의 목표 각도를 안전 학습 범위로 제한해 저장한다.
  void on_target(const std_msgs::msg::Float64::SharedPtr message)
  {
    target_position_rad_ = std::clamp(message->data, -1.0, 1.0);
    target_received_ = true;
  }

  // 선형 예측 모델 x_{k+1}=A*x_k+B*u_k를 한 스텝 전개한다.
  // A=[[1,dt],[0,1-b*dt/I]], B=[0.5*dt^2/I, dt/I]^T 이며,
  // MPC는 계산량을 제한하려고 sin(theta) 중력을 모델에서 생략한다. 실제 Plant와의 이 차이가
  // model mismatch이고, 피드백으로 매 5 ms 다시 풀기 때문에 오차를 계속 보정할 수 있다.
  static State predict(const State & state, const double torque_nm)
  {
    return State{
      state.position_rad + kDtSec * state.velocity_rad_s +
      kHalfDtSquaredOverInertia * torque_nm,
      kVelocityDecay * state.velocity_rad_s + kDtOverInertia * torque_nm};
  }

  // 고정 길이 배열에 상태 trajectory를 저장한다. std::array는 heap 할당을 하지 않는다.
  void rollout_trajectory()
  {
    predicted_states_[0] = measured_state_;
    for (std::size_t k = 0; k < kHorizon; ++k) {
      predicted_states_[k + 1] = predict(predicted_states_[k], torque_sequence_[k]);
    }
  }

  // 비용함수 J=sum(q_p*(theta-r)^2+q_v*omega^2+r_u*u^2)+terminal_cost의
  // gradient를 adjoint(lambda) 역전파로 O(N)에 계산한다.
  void compute_gradient()
  {
    const State & terminal = predicted_states_[kHorizon];
    double lambda_position = 2.0 * kTerminalPositionWeight *
      (terminal.position_rad - target_position_rad_);
    double lambda_velocity = 2.0 * kTerminalVelocityWeight * terminal.velocity_rad_s;

    for (std::size_t reverse = kHorizon; reverse > 0; --reverse) {
      const std::size_t k = reverse - 1;

      // dJ/du_k = 2*R*u_k + B^T*lambda_{k+1}.
      gradient_[k] = 2.0 * kTorqueWeight * torque_sequence_[k] +
        kHalfDtSquaredOverInertia * lambda_position +
        kDtOverInertia * lambda_velocity;

      const State & state = predicted_states_[k];
      const double next_lambda_position =
        2.0 * kPositionWeight * (state.position_rad - target_position_rad_) + lambda_position;
      const double next_lambda_velocity =
        2.0 * kVelocityWeight * state.velocity_rad_s +
        kDtSec * lambda_position + kVelocityDecay * lambda_velocity;
      lambda_position = next_lambda_position;
      lambda_velocity = next_lambda_velocity;
    }
  }

  // 토크 hard limit와 인접 샘플 간 slew-rate limit를 순서대로 투영한다.
  // |u_k|<=2.0 N*m, |u_k-u_{k-1}|<=30 N*m/s * 0.005 s가 코드 제약식이다.
  void project_constraints()
  {
    double previous = last_applied_command_nm_;
    for (double & torque_nm : torque_sequence_) {
      torque_nm = std::clamp(torque_nm, -kControllerTorqueLimitNm, kControllerTorqueLimitNm);
      torque_nm = std::clamp(torque_nm, previous - kMaxTorqueStepNm, previous + kMaxTorqueStepNm);
      previous = torque_nm;
    }
  }

  // 매 주기 같은 상한 시간에 끝나도록 정확히 12번만 projected-gradient를 반복한다.
  double solve_bounded_mpc()
  {
    // 이전 해를 한 칸 당기는 warm start는 연속된 MPC 문제의 유사성을 이용한다.
    for (std::size_t k = 0; k + 1 < kHorizon; ++k) {
      torque_sequence_[k] = torque_sequence_[k + 1];
    }
    torque_sequence_[kHorizon - 1] = torque_sequence_[kHorizon - 2];
    project_constraints();

    for (std::size_t iteration = 0; iteration < kSolverIterations; ++iteration) {
      rollout_trajectory();
      compute_gradient();
      for (std::size_t k = 0; k < kHorizon; ++k) {
        torque_sequence_[k] -= kGradientStep * gradient_[k];
      }
      project_constraints();
    }
    return torque_sequence_[0];
  }

  // jitter와 callback 실행시간을 고정 크기 histogram에 누적한다.
  void record_timing(
    const std::chrono::steady_clock::time_point start,
    const std::chrono::steady_clock::time_point finish)
  {
    // Timer의 첫 실제 시작 시각을 기준점으로 삼는다. 생성자 시각을 쓰면 DDS/Executor 설정
    // 시간이 첫 jitter 표본과 이후 전체 표본에 상수 offset으로 잘못 남는다.
    if (!timing_initialized_) {
      expected_start_ = start;
      timing_initialized_ = true;
    }
    const auto signed_jitter_us =
      std::chrono::duration_cast<std::chrono::microseconds>(start - expected_start_).count();
    const std::uint64_t jitter_us = static_cast<std::uint64_t>(std::llabs(signed_jitter_us));
    const std::uint64_t execution_us = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count());

    max_jitter_us_ = std::max(max_jitter_us_, jitter_us);
    max_execution_us_ = std::max(max_execution_us_, execution_us);

    // 경계 [50,100,250,500,1000] us와 그 이상, 총 6개 bin이다.
    const std::array<std::uint64_t, 5> boundaries_us{50, 100, 250, 500, 1000};
    std::size_t bin = 0;
    while (bin < boundaries_us.size() && jitter_us >= boundaries_us[bin]) {
      ++bin;
    }
    ++jitter_histogram_[bin];
    if (execution_us > kExecutionBudgetUs || jitter_us > kJitterBudgetUs) {
      ++budget_miss_count_;
    }

    // timer의 이상적 다음 시작점을 5 ms씩 전진시킨다. 누적 drift를 숨기지 않는다.
    expected_start_ += 5ms;
  }

  // 저주기 diagnostics는 문자열 할당을 허용하되 1 Hz로 제한한다.
  // 제어 계산용 배열과 분리해 "hard RT 경로에는 heap/I/O 금지" 원칙을 보여준다.
  void publish_diagnostics()
  {
    std::ostringstream stream;
    stream << "samples=" << control_cycle_count_
           << " max_jitter_us=" << max_jitter_us_
           << " max_execution_us=" << max_execution_us_
           << " budget_misses=" << budget_miss_count_
           << " jitter_bins_lt_50_100_250_500_1000_ge=";
    for (std::size_t i = 0; i < jitter_histogram_.size(); ++i) {
      stream << (i == 0 ? "[" : ",") << jitter_histogram_[i];
    }
    stream << "]";

    std_msgs::msg::String message;
    message.data = stream.str();
    diagnostics_publisher_->publish(message);
    RCLCPP_INFO(
      get_logger(), "target=%+.3f, theta=%+.3f, torque=%+.3f | %s",
      target_position_rad_, measured_state_.position_rad, last_applied_command_nm_,
      message.data.c_str());
  }

  // 200 Hz 제어 callback: 입력 확인 → MPC 풀이 → 첫 토크 발행 → 시간 예산 측정 순서다.
  void control_step()
  {
    const auto start = std::chrono::steady_clock::now();
    double torque_nm = 0.0;
    if (state_received_ && target_received_) {
      torque_nm = solve_bounded_mpc();
    }

    torque_message_.data = torque_nm;
    torque_publisher_->publish(torque_message_);
    last_applied_command_nm_ = torque_nm;

    const auto finish = std::chrono::steady_clock::now();
    record_timing(start, finish);
    ++control_cycle_count_;
    if (control_cycle_count_ % 200U == 0U) {
      publish_diagnostics();
    }
  }

  static constexpr std::size_t kHorizon = 20;
  static constexpr std::size_t kSolverIterations = 12;
  static constexpr double kDtSec = 0.005;
  static constexpr double kInertiaKgM2 = 0.08;
  static constexpr double kModelDamping = 0.12;
  static constexpr double kHalfDtSquaredOverInertia =
    0.5 * kDtSec * kDtSec / kInertiaKgM2;
  static constexpr double kDtOverInertia = kDtSec / kInertiaKgM2;
  static constexpr double kVelocityDecay = 1.0 - kModelDamping * kDtSec / kInertiaKgM2;
  static constexpr double kPositionWeight = 25.0;
  static constexpr double kVelocityWeight = 0.8;
  static constexpr double kTorqueWeight = 0.08;
  static constexpr double kTerminalPositionWeight = 80.0;
  static constexpr double kTerminalVelocityWeight = 2.0;
  static constexpr double kGradientStep = 0.035;
  static constexpr double kControllerTorqueLimitNm = 2.0;
  static constexpr double kTorqueSlewRateNmPerSec = 30.0;
  static constexpr double kMaxTorqueStepNm = kTorqueSlewRateNmPerSec * kDtSec;
  static constexpr std::uint64_t kExecutionBudgetUs = 1000;
  static constexpr std::uint64_t kJitterBudgetUs = 2500;

  State measured_state_{0.0, 0.0};
  double target_position_rad_{0.0};
  bool state_received_{false};
  bool target_received_{false};
  double last_applied_command_nm_{0.0};
  std::array<double, kHorizon> torque_sequence_{};
  std::array<double, kHorizon> gradient_{};
  std::array<State, kHorizon + 1> predicted_states_{};
  std_msgs::msg::Float64 torque_message_;

  std::chrono::steady_clock::time_point expected_start_;
  bool timing_initialized_{false};
  std::array<std::uint64_t, 6> jitter_histogram_{};
  std::uint64_t control_cycle_count_{0};
  std::uint64_t budget_miss_count_{0};
  std::uint64_t max_jitter_us_{0};
  std::uint64_t max_execution_us_{0};

  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr state_subscription_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr target_subscription_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr torque_publisher_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diagnostics_publisher_;
  rclcpp::TimerBase::SharedPtr control_timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto controller = std::make_shared<ConstrainedMpcController>();

  // StaticSingleThreadedExecutor는 노드 graph를 반복 탐색하는 비용을 줄인 단일 스레드
  // Executor다. 노드/Callback 수가 실행 중 고정된 제어 프로세스의 예측 가능성을 높인다.
  rclcpp::executors::StaticSingleThreadedExecutor executor;
  executor.add_node(controller);
  executor.spin();

  rclcpp::shutdown();
  return 0;
}
