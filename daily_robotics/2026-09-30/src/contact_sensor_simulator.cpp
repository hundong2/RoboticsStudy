#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_09_30/msg/contact_truth.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

// ContactSensorSimulator의 역할:
// 한 발이 AIR -> STABLE -> 마찰 저하 -> SLIP -> RECOVERY를 반복하도록 힘/속도 센서를 만든다.
// 실제 로봇에서는 force-torque sensor와 foot kinematics가 이 노드를 대체한다.
class ContactSensorSimulator : public rclcpp::Node
{
public:
  ContactSensorSimulator()
  : Node("contact_sensor_simulator"), started_at_(std::chrono::steady_clock::now())
  {
    // SensorDataQoS는 센서의 최신값을 우선하는 best-effort/짧은 history 프로파일이다.
    // 500 Hz 데이터에서 오래된 표본을 끝까지 전달하는 것보다 최신 접촉 상태가 더 중요하다.
    wrench_publisher_ = create_publisher<geometry_msgs::msg::WrenchStamped>(
      "/foot/wrench", rclcpp::SensorDataQoS().keep_last(5));
    velocity_publisher_ = create_publisher<geometry_msgs::msg::TwistStamped>(
      "/foot/twist", rclcpp::SensorDataQoS().keep_last(5));

    // 제어 명령과 ground truth는 유실 없이 감사를 해야 하므로 reliable QoS를 사용한다.
    request_publisher_ = create_publisher<geometry_msgs::msg::WrenchStamped>(
      "/controller/requested_wrench", rclcpp::QoS(10).reliable());
    truth_publisher_ = create_publisher<daily_robotics_2026_09_30::msg::ContactTruth>(
      "/sim/contact_truth", rclcpp::QoS(10).reliable());

    // 2 ms wall timer는 500 Hz 센서 주기를 모사한다. wall timer는 /clock 정지와 무관하게 진행된다.
    timer_ = create_wall_timer(2ms, [this]() {publish_sample();});
  }

private:
  struct Scenario
  {
    std::uint8_t mode{0U};
    double mu{0.8};
    double normal_force{0.0};
    double requested_tangent{0.0};
  };

  // scenario_at은 시간에 따른 환경의 "정답"을 만든다. 분기 수가 고정이라 실행시간이 입력에 따라 폭증하지 않는다.
  static Scenario scenario_at(const double seconds)
  {
    Scenario value;
    if (seconds < 1.5) {
      value = Scenario{0U, 0.8, 0.0, 0.0};
    } else if (seconds < 4.5) {
      value = Scenario{1U, 0.8, 120.0, 30.0};
    } else if (seconds < 7.5) {
      // 마찰계수 mu가 0.8 -> 0.32로 내려가고 요구 접선력은 30 -> 65 N으로 증가한다.
      const double ratio = (seconds - 4.5) / 3.0;
      value = Scenario{1U, 0.8 - 0.48 * ratio, 120.0, 30.0 + 35.0 * ratio};
    } else if (seconds < 9.5) {
      value = Scenario{2U, 0.32, 110.0, 65.0};
    } else {
      value = Scenario{1U, 0.75, 120.0, 25.0};
    }
    return value;
  }

  void publish_sample()
  {
    const auto steady_now = std::chrono::steady_clock::now();
    const double total_seconds =
      std::chrono::duration<double>(steady_now - started_at_).count();
    // fmod로 12초 시나리오를 반복해 smoke test 시작이 조금 늦어도 모든 상태를 다시 볼 수 있게 한다.
    const double phase_seconds = std::fmod(total_seconds, 12.0);
    const Scenario scenario = scenario_at(phase_seconds);

    // Coulomb 마찰 원뿔 |F_t| <= mu*F_n의 92%까지만 실제 접선력이 전달된다고 모사한다.
    // 여유 8%는 "경계에 닿기 전부터 미끄럼 징후가 나타나는" 현실적인 안전 버퍼다.
    const double traction_limit = 0.92 * scenario.mu * scenario.normal_force;
    const double actual_tangent = std::min(scenario.requested_tangent, traction_limit);
    const double overload = std::max(0.0, scenario.requested_tangent - traction_limit);
    // v_slip = overload / 350은 단순한 교육용 점성 근사이며 정밀 접촉 모델은 아니다.
    const double slip_speed = (scenario.normal_force > 1.0) ? overload / 350.0 : 0.0;
    // 센서 잡음보다 큰 0.0185 m/s부터 ground truth의 "위험 slip"으로 기록한다.
    // 감독 threshold(0.025 m/s)보다 낮아 감사기가 실제 위험 시작부터 trip latency를 측정할 수 있다.
    const std::uint8_t actual_mode = slip_speed > 0.0185 ? 2U : scenario.mode;

    // 같은 timestamp를 힘/속도/정답에 복사해야 수신기가 서로 같은 물리 표본인지 확인할 수 있다.
    const rclcpp::Time stamp = now();
    ++sample_index_;
    const double deterministic_noise = 0.18 * std::sin(static_cast<double>(sample_index_) * 0.071);

    geometry_msgs::msg::WrenchStamped wrench;
    wrench.header.stamp = stamp;
    wrench.header.frame_id = "left_foot_contact";
    wrench.wrench.force.x = actual_tangent + deterministic_noise;
    wrench.wrench.force.z = scenario.normal_force + 0.4 * deterministic_noise;
    wrench_publisher_->publish(wrench);

    geometry_msgs::msg::TwistStamped twist;
    twist.header = wrench.header;
    twist.twist.linear.x = slip_speed + 0.0002 * std::sin(static_cast<double>(sample_index_) * 0.13);
    velocity_publisher_->publish(twist);

    geometry_msgs::msg::WrenchStamped request;
    request.header = wrench.header;
    request.wrench.force.x = scenario.requested_tangent;
    request_publisher_->publish(request);

    daily_robotics_2026_09_30::msg::ContactTruth truth;
    truth.header = wrench.header;
    truth.mode = actual_mode;
    truth.friction_coefficient = scenario.mu;
    truth.normal_force_n = scenario.normal_force;
    truth.requested_tangential_force_n = scenario.requested_tangent;
    truth.actual_tangential_force_n = actual_tangent;
    truth.slip_speed_mps = slip_speed;
    truth_publisher_->publish(truth);
  }

  std::chrono::steady_clock::time_point started_at_;
  std::uint64_t sample_index_{0U};
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr wrench_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr velocity_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr request_publisher_;
  rclcpp::Publisher<daily_robotics_2026_09_30::msg::ContactTruth>::SharedPtr truth_publisher_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 ROS 인자, DDS context, signal handler를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 executor가 timer callback을 실행하게 한다. 이 노드는 callback 하나라 단일 스레드가 충분하다.
  rclcpp::spin(std::make_shared<ContactSensorSimulator>());
  rclcpp::shutdown();
  return 0;
}
