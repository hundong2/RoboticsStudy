#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64.hpp"

using namespace std::chrono_literals;

// 이 노드는 두 역할을 함께 수행한다.
// 1) raw 토크에 포화·watchdog·소프트 리미트를 적용하는 독립 안전 경계,
// 2) I*theta_ddot + b*theta_dot + k_g*sin(theta) = tau 인 1축 관절 플랜트 시뮬레이터.
class SafeJointPlant final : public rclcpp::Node
{
public:
  SafeJointPlant()
  : Node("safe_joint_plant"), last_command_time_(std::chrono::steady_clock::now())
  {
    // 명령은 최신 하나만 필요하고 유실보다 일관성이 중요하므로 Reliable KeepLast(1)을 쓴다.
    const auto command_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
    torque_subscription_ = create_subscription<std_msgs::msg::Float64>(
      "/motor/torque_raw", command_qos,
      std::bind(&SafeJointPlant::on_torque_command, this, std::placeholders::_1));

    // JointState는 고주기 센서 데이터이므로 오래된 표본 재전송보다 최신성이 중요한
    // SensorDataQoS(Best Effort, 작은 depth)를 사용한다.
    state_publisher_ = create_publisher<sensor_msgs::msg::JointState>(
      "/joint/state", rclcpp::SensorDataQoS());
    applied_torque_publisher_ = create_publisher<std_msgs::msg::Float64>(
      "/motor/torque_applied", rclcpp::QoS(1).reliable());

    // 1 ms 주기는 수치 적분(1 kHz), 5 ms 주기는 센서 발행(200 Hz)을 분리한다.
    simulation_timer_ = create_wall_timer(1ms, std::bind(&SafeJointPlant::simulate_one_step, this));
    state_timer_ = create_wall_timer(5ms, std::bind(&SafeJointPlant::publish_state, this));
    log_timer_ = create_wall_timer(1s, std::bind(&SafeJointPlant::log_status, this));

    // vector가 첫 고주기 publish에서 메모리를 할당하지 않도록 크기를 한 번만 확정한다.
    state_message_.name.resize(1);
    state_message_.position.resize(1);
    state_message_.velocity.resize(1);
    state_message_.effort.resize(1);
    state_message_.name[0] = "joint_1";
  }

private:
  // MPC가 계산한 토크와 수신 시각을 저장한다. SingleThreadedExecutor에서 실행되므로
  // 이 callback과 simulation timer는 동시에 접근하지 않아 별도 mutex가 필요 없다.
  void on_torque_command(const std_msgs::msg::Float64::SharedPtr message)
  {
    requested_torque_nm_ = message->data;
    last_command_time_ = std::chrono::steady_clock::now();
    command_received_ = true;
  }

  // 안전 제약을 적용한 뒤 한 스텝의 관절 동역학을 semi-implicit Euler로 적분한다.
  void simulate_one_step()
  {
    const auto now = std::chrono::steady_clock::now();
    const bool command_stale = !command_received_ || (now - last_command_time_) > 50ms;

    // Controller 장애나 통신 단절이 50 ms를 넘으면 토크를 0으로 내려 fail-safe 한다.
    double safe_torque_nm = command_stale ? 0.0 : requested_torque_nm_;
    if (command_stale && command_received_ && !watchdog_active_) {
      ++watchdog_trip_count_;
    }
    watchdog_active_ = command_stale;

    // 액추에이터 정격을 흉내 낸 hard saturation: |tau| <= 2.5 N*m.
    safe_torque_nm = std::clamp(safe_torque_nm, -kMaxTorqueNm, kMaxTorqueNm);

    // |theta|가 soft limit를 넘은 상태에서 더 바깥쪽으로 미는 토크만 차단한다.
    // 실제 제품은 이 소프트웨어 경계 외에도 드라이브/하드웨어 limit가 반드시 필요하다.
    if ((position_rad_ >= kSoftPositionLimitRad && safe_torque_nm > 0.0) ||
      (position_rad_ <= -kSoftPositionLimitRad && safe_torque_nm < 0.0))
    {
      safe_torque_nm = 0.0;
    }

    // 수식: theta_ddot = (tau - b*theta_dot - k_g*sin(theta)) / I.
    // sin(theta) 항은 수평축 링크의 중력 토크를 단순화한 비선형 항이다.
    const double acceleration_rad_s2 =
      (safe_torque_nm - kViscousDamping * velocity_rad_s_ -
      kGravityTorqueNm * std::sin(position_rad_)) / kInertiaKgM2;

    // Semi-implicit Euler: 먼저 omega_{k+1}=omega_k+alpha*dt를 구한 뒤
    // theta_{k+1}=theta_k+omega_{k+1}*dt로 적분해 explicit Euler보다 안정성을 높인다.
    velocity_rad_s_ += acceleration_rad_s2 * kSimulationDtSec;
    velocity_rad_s_ = std::clamp(velocity_rad_s_, -kMaxVelocityRadS, kMaxVelocityRadS);
    position_rad_ += velocity_rad_s_ * kSimulationDtSec;
    applied_torque_nm_ = safe_torque_nm;
  }

  // 표준 JointState로 위치·속도·실제 적용 토크를 발행해 Controller 피드백으로 제공한다.
  void publish_state()
  {
    state_message_.header.stamp = now();
    state_message_.position[0] = position_rad_;
    state_message_.velocity[0] = velocity_rad_s_;
    state_message_.effort[0] = applied_torque_nm_;
    state_publisher_->publish(state_message_);

    applied_torque_message_.data = applied_torque_nm_;
    applied_torque_publisher_->publish(applied_torque_message_);
  }

  // 사람용 로그는 1 Hz로 제한해 고주기 제어 경로의 console I/O 간섭을 줄인다.
  void log_status()
  {
    RCLCPP_INFO(
      get_logger(), "theta=%+.3f rad, omega=%+.3f rad/s, raw=%+.3f N*m, "
      "applied=%+.3f N*m, watchdog=%s, trips=%llu",
      position_rad_, velocity_rad_s_, requested_torque_nm_, applied_torque_nm_,
      watchdog_active_ ? "ACTIVE" : "ok",
      static_cast<unsigned long long>(watchdog_trip_count_));
  }

  static constexpr double kSimulationDtSec = 0.001;
  static constexpr double kInertiaKgM2 = 0.08;
  static constexpr double kViscousDamping = 0.12;
  static constexpr double kGravityTorqueNm = 0.55;
  static constexpr double kMaxTorqueNm = 2.5;
  static constexpr double kSoftPositionLimitRad = 1.2;
  static constexpr double kMaxVelocityRadS = 4.0;

  double position_rad_{0.0};
  double velocity_rad_s_{0.0};
  double requested_torque_nm_{0.0};
  double applied_torque_nm_{0.0};
  bool command_received_{false};
  bool watchdog_active_{true};
  std::uint64_t watchdog_trip_count_{0};
  std::chrono::steady_clock::time_point last_command_time_;

  sensor_msgs::msg::JointState state_message_;
  std_msgs::msg::Float64 applied_torque_message_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr torque_subscription_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr state_publisher_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr applied_torque_publisher_;
  rclcpp::TimerBase::SharedPtr simulation_timer_;
  rclcpp::TimerBase::SharedPtr state_timer_;
  rclcpp::TimerBase::SharedPtr log_timer_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor를 내부 생성하는 간단한 spin은 상태와 timer 접근을 직렬화한다.
  rclcpp::spin(std::make_shared<SafeJointPlant>());
  rclcpp::shutdown();
  return 0;
}
