#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

#include "daily_robotics_2026_09_23/msg/effort_command.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float64.hpp"

namespace
{
constexpr double kMass = 1.0;
constexpr double kJointDamping = 1.2;
constexpr double kVirtualStiffness = 60.0;
// D = 2*zeta*sqrt(M*K), zeta≈0.9에 가까운 값으로 접촉 시 과도 진동을 줄인다.
constexpr double kVirtualDamping = 14.0;
constexpr double kNominalEffortLimit = 12.0;
}  // namespace

/**
 * @brief JointState를 받을 때마다 임피던스 힘과 외란 관측값을 계산해 원시 힘 명령을 발행한다.
 *
 * 제어 법칙은 tau = K(q_d-q) + D(v_d-v)이다. 위치를 강제로 맞추는 대신 목표 위치 오차에 비례한
 * 가상 스프링과 속도에 비례한 가상 댐퍼를 만들기 때문에, 벽을 만나면 오차가 접촉력으로 바뀐다.
 */
class ImpedanceController final : public rclcpp::Node
{
public:
  ImpedanceController()
  : Node("impedance_controller"), start_(std::chrono::steady_clock::now())
  {
    command_pub_ = create_publisher<daily_robotics_2026_09_23::msg::EffortCommand>(
      "/joint_effort_raw", rclcpp::QoS(4).reliable());

    target_sub_ = create_subscription<std_msgs::msg::Float64>(
      "/contact/target", rclcpp::QoS(1).reliable(),
      [this](const std_msgs::msg::Float64::SharedPtr msg) {
        desired_position_ = msg->data;
      });

    // WrenchStamped는 Header로 측정 시각/좌표계를, Wrench로 힘·토크를 함께 전달한다.
    wrench_sub_ = create_subscription<geometry_msgs::msg::WrenchStamped>(
      "/contact/wrench", rclcpp::SensorDataQoS(),
      [this](const geometry_msgs::msg::WrenchStamped::SharedPtr msg) {
        latest_measured_force_ = msg->wrench.force.x;
      });

    // JointState 콜백이 제어 주기를 주도한다. name/position/velocity 배열 계약을 먼저 검사해
    // 잘못된 센서 메시지가 배열 범위를 벗어나거나 엉뚱한 관절을 움직이지 못하게 한다.
    joint_sub_ = create_subscription<sensor_msgs::msg::JointState>(
      "/contact/joint_state", rclcpp::SensorDataQoS(),
      std::bind(&ImpedanceController::on_joint_state, this, std::placeholders::_1));
  }

private:
  /** @brief 고정 횟수 산술만 수행하는 500 Hz 제어 hot path이며, 로깅·파일 I/O·대기 호출을 하지 않는다. */
  void on_joint_state(const sensor_msgs::msg::JointState::SharedPtr msg)
  {
    const auto begin = std::chrono::steady_clock::now();
    if (msg->name.size() != 1 || msg->name[0] != "contact_axis_joint" ||
      msg->position.size() != 1 || msg->velocity.size() != 1)
    {
      return;
    }

    const double q = msg->position[0];
    const double v = msg->velocity[0];
    // Header.stamp를 ns로 바꾸어 센서 측정 간격을 계산한다. 도착 시각 차이를 쓰지 않으므로 DDS 지터가
    // 가속도 추정식에 직접 섞이지 않는다.
    const std::int64_t stamp_ns =
      static_cast<std::int64_t>(msg->header.stamp.sec) * 1000000000LL +
      static_cast<std::int64_t>(msg->header.stamp.nanosec);
    const double dt = last_stamp_ns_ > 0 ?
      std::clamp(static_cast<double>(stamp_ns - last_stamp_ns_) * 1.0e-9, 0.0005, 0.010) : 0.002;

    // q_ddot ≈ (v_k-v_{k-1})/dt, F_ext = M*q_ddot - tau + b*q_dot.
    // 이 식은 플랜트 운동방정식을 외력에 대해 다시 푼 것이며, 1차 저역통과로 차분 잡음을 줄인다.
    const double acceleration = (v - last_velocity_) / dt;
    const double raw_disturbance = kMass * acceleration - last_effort_ + kJointDamping * v;
    constexpr double kObserverAlpha = 0.08;
    estimated_contact_force_ +=
      kObserverAlpha * (raw_disturbance - estimated_contact_force_);

    // tau = K(q_d-q) + D(0-v): 목표 속도를 0으로 두는 1축 임피던스 제어식이다.
    double effort = kVirtualStiffness * (desired_position_ - q) - kVirtualDamping * v;
    effort = std::clamp(effort, -kNominalEffortLimit, kNominalEffortLimit);

    const double elapsed = std::chrono::duration<double>(begin - start_).count();
    // 4.00~4.14 s에만 과도 힘을 주입한다. 이는 안전 게이트의 차단/복구를 재현하는 시험 결함이며,
    // 실제 제어 알고리즘의 일부가 아니다.
    if (elapsed >= 4.00 && elapsed < 4.14) {
      effort = 35.0;
    }

    const auto finish = std::chrono::steady_clock::now();
    const double hot_path_us = std::chrono::duration<double, std::micro>(finish - begin).count();

    daily_robotics_2026_09_23::msg::EffortCommand command;
    const auto ros_stamp = now();
    const std::int64_t ros_ns = ros_stamp.nanoseconds();
    command.stamp.sec = static_cast<std::int32_t>(ros_ns / 1000000000LL);
    command.stamp.nanosec = static_cast<std::uint32_t>(ros_ns % 1000000000LL);
    command.sequence = sequence_++;
    command.effort = effort;
    command.desired_position = desired_position_;
    command.estimated_contact_force = estimated_contact_force_;
    command.hot_path_us = hot_path_us;
    command_pub_->publish(command);

    last_stamp_ns_ = stamp_ns;
    last_velocity_ = v;
    last_effort_ = effort;
    (void)latest_measured_force_;  // 실측 힘은 관측기 비교 학습용이며 제어 법칙에는 넣지 않는다.
  }

  rclcpp::Publisher<daily_robotics_2026_09_23::msg::EffortCommand>::SharedPtr command_pub_;
  rclcpp::Subscription<std_msgs::msg::Float64>::SharedPtr target_sub_;
  rclcpp::Subscription<geometry_msgs::msg::WrenchStamped>::SharedPtr wrench_sub_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_sub_;

  std::chrono::steady_clock::time_point start_;
  std::uint64_t sequence_{0};
  std::int64_t last_stamp_ns_{0};
  double desired_position_{0.55};
  double latest_measured_force_{0.0};
  double estimated_contact_force_{0.0};
  double last_velocity_{0.0};
  double last_effort_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor인 spin은 세 구독 콜백의 상태 갱신을 직렬화한다.
  rclcpp::spin(std::make_shared<ImpedanceController>());
  rclcpp::shutdown();
  return 0;
}
