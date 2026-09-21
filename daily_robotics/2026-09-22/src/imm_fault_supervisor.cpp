#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "diagnostic_msgs/msg/key_value.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64.hpp"

using namespace std::chrono_literals;

// 이 노드는 엔코더와 명령의 물리적 일관성을 2모델 IMM으로 검사한다. 출력저하 확률이나
// 통신 신선도 계약이 위험하면 전달 명령을 0으로 만들고, 느린 /diagnostics 경로로 근거를 낸다.
class ImmFaultSupervisor final : public rclcpp::Node
{
public:
  ImmFaultSupervisor()
  : Node("imm_fault_supervisor")
  {
    requested_command_subscription_ = create_subscription<std_msgs::msg::Float64>(
      "/actuator/requested_command", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
      std::bind(&ImmFaultSupervisor::on_requested_command, this, std::placeholders::_1));

    // Publisher와 Reliability/Deadline/Liveliness를 동일하게 맞춰 QoS 비호환을 피한다.
    rclcpp::QoS state_qos(rclcpp::KeepLast(5));
    state_qos.reliable()
      .deadline(15ms)
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(50ms);

    rclcpp::SubscriptionOptions state_options;
    // Requested deadline은 샘플 간격 계약이다. 위반 횟수는 진단 근거로 누적한다.
    state_options.event_callbacks.deadline_callback =
      [this](rclcpp::QOSDeadlineRequestedInfo & info) {
        if (info.total_count_change > 0) {
          deadline_misses_ += static_cast<std::uint64_t>(info.total_count_change);
        }
      };
    // Liveliness changed는 Publisher가 lease를 갱신하지 못한 장치 heartbeat 장애를 뜻한다.
    state_options.event_callbacks.liveliness_callback =
      [this](rclcpp::QOSLivelinessChangedInfo & info) {
        if (info.not_alive_count_change > 0) {
          liveliness_losses_ += static_cast<std::uint64_t>(info.not_alive_count_change);
        }
      };

    joint_state_subscription_ = create_subscription<sensor_msgs::msg::JointState>(
      "/joint_states", state_qos,
      std::bind(&ImmFaultSupervisor::on_joint_state, this, std::placeholders::_1),
      state_options);

    safe_command_publisher_ = create_publisher<std_msgs::msg::Float64>(
      "/actuator/safe_command",
      rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());
    diagnostics_publisher_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local());

    // 200 Hz 경로에는 고정 크기 IMM 계산과 scalar publish만 둔다.
    hot_path_timer_ = create_wall_timer(5ms, std::bind(&ImmFaultSupervisor::on_hot_path, this));
    // 문자열과 vector를 만드는 DiagnosticArray는 20 Hz 비실시간 경로로 분리한다.
    diagnostics_timer_ = create_wall_timer(
      50ms, std::bind(&ImmFaultSupervisor::publish_diagnostics, this));

    last_sample_steady_ = std::chrono::steady_clock::now();
    RCLCPP_INFO(get_logger(), "two-model IMM supervisor started (healthy gain=1, degraded gain=0.25)");
  }

private:
  struct ScalarFilter
  {
    double state{0.0};
    double variance{1.0};
  };

  enum class SafetyState
  {
    BOOTSTRAP,
    NORMAL,
    SUSPECT,
    SAFE_STOP,
    RECOVERING
  };

  enum class StopReason
  {
    NONE,
    ACTUATOR,
    TRANSPORT
  };

  // 최신 목표를 저장한다. 이 실습은 SingleThreadedExecutor라 같은 멤버를 동시에 쓰지 않는다.
  void on_requested_command(const std_msgs::msg::Float64::SharedPtr message)
  {
    requested_command_rad_s_ = message->data;
    command_received_ = true;
  }

  // JointState 문법을 검증하고 최신 속도만 O(1)로 복사한다. 벡터가 비어 있으면 폐기한다.
  void on_joint_state(const sensor_msgs::msg::JointState::SharedPtr message)
  {
    if (message->velocity.empty()) {
      ++malformed_samples_;
      return;
    }
    measured_velocity_rad_s_ = message->velocity.front();
    last_sample_steady_ = std::chrono::steady_clock::now();
    sample_received_ = true;
    new_sample_ = true;
  }

  // 정상/출력저하 모델의 상태를 섞고, 각각 Kalman predict/correct 후 모델 확률을 갱신한다.
  // 배열 크기는 항상 2라서 입력 데이터에 따라 반복 횟수나 heap 사용량이 커지지 않는다.
  void update_imm(double command, double measurement)
  {
    constexpr std::size_t model_count = 2U;
    constexpr double dt = 0.005;
    constexpr double tau_s = 0.12;
    constexpr double process_variance = 0.003;
    constexpr double measurement_variance = 0.0009;
    constexpr double two_pi = 6.28318530717958647692;
    constexpr std::array<double, model_count> gains{1.0, 0.25};
    // p_ij=P(mode_k=j | mode_{k-1}=i). 정상은 오래 유지되고 고장은 복구 가능하게 둔다.
    constexpr std::array<std::array<double, model_count>, model_count> transition{{
      {{0.995, 0.005}},
      {{0.020, 0.980}}
    }};

    const double decay = std::exp(-dt / tau_s);
    std::array<double, model_count> prior_mode_probability{};
    std::array<double, model_count> mixed_state{};
    std::array<double, model_count> mixed_variance{};

    // c_j = sum_i p_ij * mu_i 는 현재 모드 j의 사전확률이다.
    for (std::size_t j = 0; j < model_count; ++j) {
      for (std::size_t i = 0; i < model_count; ++i) {
        prior_mode_probability[j] += transition[i][j] * mode_probability_[i];
      }
      prior_mode_probability[j] = std::max(prior_mode_probability[j], 1.0e-12);

      // mu_{i|j}=p_ij*mu_i/c_j 로 이전 모델 상태를 현재 모델의 초기조건으로 섞는다.
      for (std::size_t i = 0; i < model_count; ++i) {
        const double mixing_weight =
          transition[i][j] * mode_probability_[i] / prior_mode_probability[j];
        mixed_state[j] += mixing_weight * filters_[i].state;
      }
      for (std::size_t i = 0; i < model_count; ++i) {
        const double mixing_weight =
          transition[i][j] * mode_probability_[i] / prior_mode_probability[j];
        const double delta = filters_[i].state - mixed_state[j];
        // P0_j=sum_i mu_{i|j}(P_i+(x_i-x0_j)^2): 모델 간 평균 차이도 분산에 포함한다.
        mixed_variance[j] += mixing_weight * (filters_[i].variance + delta * delta);
      }
    }

    std::array<double, model_count> likelihood{};
    for (std::size_t j = 0; j < model_count; ++j) {
      // 각 모델의 물리식: v(k+1)=a*v(k)+(1-a)*g_j*u(k)+w(k).
      const double predicted_state =
        decay * mixed_state[j] + (1.0 - decay) * gains[j] * command;
      const double predicted_variance =
        decay * decay * mixed_variance[j] + process_variance;
      const double innovation = measurement - predicted_state;
      const double innovation_variance = predicted_variance + measurement_variance;

      // 1차원 Gaussian likelihood N(r;0,S). residual이 작은 모델의 확률이 커진다.
      likelihood[j] = std::max(
        std::exp(-0.5 * innovation * innovation / innovation_variance) /
        std::sqrt(two_pi * innovation_variance),
        1.0e-300);

      // K=P-/S, x=x-+K*r, P=(1-K)P- 는 scalar Kalman correction이다.
      const double kalman_gain = predicted_variance / innovation_variance;
      filters_[j].state = predicted_state + kalman_gain * innovation;
      filters_[j].variance = std::max(
        (1.0 - kalman_gain) * predicted_variance, 1.0e-12);
    }

    // mu_j(k)=Lambda_j*c_j / sum_l Lambda_l*c_l 로 Bayesian 모드 확률을 정규화한다.
    double normalization = 0.0;
    for (std::size_t j = 0; j < model_count; ++j) {
      mode_probability_[j] = likelihood[j] * prior_mode_probability[j];
      normalization += mode_probability_[j];
    }
    if (normalization <= 1.0e-300) {
      mode_probability_ = {0.5, 0.5};
    } else {
      for (double & probability : mode_probability_) {
        probability /= normalization;
      }
    }
  }

  // IMM 확률과 transport watchdog을 hysteresis 상태 머신으로 바꿔 채터링 없는 안전 출력을 만든다.
  void update_safety_state(bool transport_fault)
  {
    constexpr std::uint32_t fault_hold_samples = 20U;    // 100 ms
    constexpr std::uint32_t recovery_hold_samples = 60U; // 300 ms
    const double degraded_probability = mode_probability_[1];

    if (degraded_probability >= 0.90) {
      ++fault_evidence_count_;
    } else {
      fault_evidence_count_ = 0U;
    }
    if (degraded_probability <= 0.20) {
      ++healthy_evidence_count_;
    } else {
      healthy_evidence_count_ = 0U;
    }

    // 통신 단절은 모델 판단을 기다릴 수 없으므로 어떤 상태에서도 즉시 SAFE_STOP으로 간다.
    if (transport_fault) {
      state_ = SafetyState::SAFE_STOP;
      stop_reason_ = StopReason::TRANSPORT;
      return;
    }

    switch (state_) {
      case SafetyState::BOOTSTRAP:
        // 초기 motor transient가 고장으로 오인되지 않도록 200개 실제 샘플(1 s)을 기다린다.
        if (processed_samples_ >= 200U && healthy_evidence_count_ >= recovery_hold_samples) {
          state_ = SafetyState::NORMAL;
          stop_reason_ = StopReason::NONE;
        }
        break;
      case SafetyState::NORMAL:
        if (fault_evidence_count_ >= fault_hold_samples) {
          state_ = SafetyState::SAFE_STOP;
          stop_reason_ = StopReason::ACTUATOR;
        } else if (degraded_probability >= 0.60) {
          state_ = SafetyState::SUSPECT;
        }
        break;
      case SafetyState::SUSPECT:
        if (fault_evidence_count_ >= fault_hold_samples) {
          state_ = SafetyState::SAFE_STOP;
          stop_reason_ = StopReason::ACTUATOR;
        } else if (healthy_evidence_count_ >= recovery_hold_samples) {
          state_ = SafetyState::NORMAL;
        }
        break;
      case SafetyState::SAFE_STOP:
        // 신선한 샘플과 정상 모델 근거가 300 ms 이어져야 RECOVERING으로 이동한다.
        if (healthy_evidence_count_ >= recovery_hold_samples) {
          state_ = SafetyState::RECOVERING;
          recovery_count_ = 0U;
        }
        break;
      case SafetyState::RECOVERING:
        if (degraded_probability >= 0.60) {
          state_ = SafetyState::SAFE_STOP;
          stop_reason_ = StopReason::ACTUATOR;
          recovery_count_ = 0U;
        } else if (++recovery_count_ >= recovery_hold_samples) {
          state_ = SafetyState::NORMAL;
          stop_reason_ = StopReason::NONE;
        }
        break;
    }
  }

  // 5 ms마다 실행되는 핵심 경로다. 새 측정은 한 번만 소비하고, stale이면 즉시 0을 출력한다.
  void on_hot_path()
  {
    const auto start = std::chrono::steady_clock::now();
    const auto now_steady = start;
    const auto sample_age = now_steady - last_sample_steady_;
    // 25 ms는 5개 정상 주기다. DDS 사건 지원 차이와 무관하게 로컬 age로도 안전을 보장한다.
    const bool transport_fault =
      !sample_received_ || sample_age > 25ms;

    if (new_sample_ && command_received_ && !transport_fault) {
      update_imm(requested_command_rad_s_, measured_velocity_rad_s_);
      new_sample_ = false;
      ++processed_samples_;
    }
    update_safety_state(transport_fault);

    // SAFE_STOP/BOOTSTRAP에서는 명령을 즉시 0으로 만든다. 정상 경로의 slew limit은
    // y(k)=y(k-1)+clamp(u-y(k-1),±0.05)로 10 rad/s^2 변화율을 제한한다.
    if (state_ == SafetyState::SAFE_STOP || state_ == SafetyState::BOOTSTRAP) {
      safe_command_rad_s_ = 0.0;
    } else {
      const double delta = std::clamp(
        requested_command_rad_s_ - safe_command_rad_s_, -0.05, 0.05);
      safe_command_rad_s_ += delta;
    }

    std_msgs::msg::Float64 output;
    output.data = safe_command_rad_s_;
    safe_command_publisher_->publish(output);

    const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start).count();
    max_hot_path_us_ = std::max(max_hot_path_us_, static_cast<std::uint64_t>(elapsed_us));
  }

  static const char * state_name(SafetyState state)
  {
    switch (state) {
      case SafetyState::BOOTSTRAP: return "BOOTSTRAP";
      case SafetyState::NORMAL: return "NORMAL";
      case SafetyState::SUSPECT: return "SUSPECT";
      case SafetyState::SAFE_STOP: return "SAFE_STOP";
      case SafetyState::RECOVERING: return "RECOVERING";
    }
    return "UNKNOWN";
  }

  static const char * reason_name(StopReason reason)
  {
    switch (reason) {
      case StopReason::NONE: return "NONE";
      case StopReason::ACTUATOR: return "ACTUATOR";
      case StopReason::TRANSPORT: return "TRANSPORT";
    }
    return "UNKNOWN";
  }

  // KeyValue는 diagnostics CLI/GUI가 이름으로 지표를 읽게 하는 표준 key-value 메시지다.
  static diagnostic_msgs::msg::KeyValue key_value(
    const std::string & key, const std::string & value)
  {
    diagnostic_msgs::msg::KeyValue output;
    output.key = key;
    output.value = value;
    return output;
  }

  // 20 Hz 비실시간 경로에서 문자열/가변 길이 vector를 만들고 운영자가 볼 진단을 게시한다.
  void publish_diagnostics()
  {
    diagnostic_msgs::msg::DiagnosticArray array;
    array.header.stamp = now();

    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = "actuator/left_wheel/fault_supervisor";
    status.hardware_id = "educational_actuator_bench";
    if (state_ == SafetyState::SAFE_STOP) {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      status.message = "fail-safe command forced to zero";
    } else if (state_ == SafetyState::SUSPECT || state_ == SafetyState::RECOVERING) {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::WARN;
      status.message = "fault evidence or guarded recovery";
    } else {
      status.level = diagnostic_msgs::msg::DiagnosticStatus::OK;
      status.message = "actuator model and transport are healthy";
    }

    status.values.reserve(13U);
    status.values.push_back(key_value("state", state_name(state_)));
    status.values.push_back(key_value("reason", reason_name(stop_reason_)));
    status.values.push_back(key_value("healthy_probability", std::to_string(mode_probability_[0])));
    status.values.push_back(key_value("degraded_probability", std::to_string(mode_probability_[1])));
    status.values.push_back(key_value("requested_rad_s", std::to_string(requested_command_rad_s_)));
    status.values.push_back(key_value("measured_rad_s", std::to_string(measured_velocity_rad_s_)));
    status.values.push_back(key_value("safe_command_rad_s", std::to_string(safe_command_rad_s_)));
    const auto age_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - last_sample_steady_).count();
    status.values.push_back(key_value("sample_age_ms", std::to_string(age_us / 1000.0)));
    status.values.push_back(key_value("deadline_misses", std::to_string(deadline_misses_)));
    status.values.push_back(key_value("liveliness_losses", std::to_string(liveliness_losses_)));
    status.values.push_back(key_value("processed_samples", std::to_string(processed_samples_)));
    status.values.push_back(key_value("malformed_samples", std::to_string(malformed_samples_)));
    status.values.push_back(key_value("max_hot_path_us", std::to_string(max_hot_path_us_)));

    array.status.push_back(std::move(status));
    diagnostics_publisher_->publish(array);
  }

  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr requested_command_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_state_subscription_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr safe_command_publisher_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostics_publisher_;
  rclcpp::TimerBase::SharedPtr hot_path_timer_;
  rclcpp::TimerBase::SharedPtr diagnostics_timer_;

  std::array<ScalarFilter, 2> filters_{};
  std::array<double, 2> mode_probability_{0.98, 0.02};
  std::chrono::steady_clock::time_point last_sample_steady_{};
  SafetyState state_{SafetyState::BOOTSTRAP};
  StopReason stop_reason_{StopReason::NONE};
  double requested_command_rad_s_{0.0};
  double measured_velocity_rad_s_{0.0};
  double safe_command_rad_s_{0.0};
  std::uint64_t processed_samples_{0U};
  std::uint64_t malformed_samples_{0U};
  std::uint64_t deadline_misses_{0U};
  std::uint64_t liveliness_losses_{0U};
  std::uint64_t max_hot_path_us_{0U};
  std::uint32_t fault_evidence_count_{0U};
  std::uint32_t healthy_evidence_count_{0U};
  std::uint32_t recovery_count_{0U};
  bool command_received_{false};
  bool sample_received_{false};
  bool new_sample_{false};
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ImmFaultSupervisor>();
  // SingleThreadedExecutor는 명령/측정/QoS/Timer 콜백이 동시에 멤버를 수정하지 않게 한다.
  // 이는 mutual exclusion을 단순화하지만 OS scheduling이나 DDS의 hard RT를 증명하지 않는다.
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
