#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

#include "daily_robotics_2026_10_21/msg/contact_sample.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kGravity = 9.80665;
constexpr double kLegLengthM = 0.55;
constexpr double kDtS = 0.005;

struct VerticalTruth
{
  double height_m;
  double velocity_mps;
  double acceleration_mps2;
};

VerticalTruth make_truth(const double time_s)
{
  // z(t)=z0+A sin(wt)를 미분하면 v=A w cos(wt), a=-A w^2 sin(wt)다.
  // 시뮬레이터와 감사 노드가 같은 수식을 공유하지 않도록 진실값 자체를 topic으로 보낸다.
  constexpr double amplitude_m = 0.025;
  constexpr double angular_rate_rad_s = 2.0 * kPi * 0.7;
  const double phase = angular_rate_rad_s * time_s;
  return {
    0.45 + amplitude_m * std::sin(phase),
    amplitude_m * angular_rate_rad_s * std::cos(phase),
    -amplitude_m * angular_rate_rad_s * angular_rate_rad_s * std::sin(phase)};
}
}  // namespace

class ContactSensorSimulator : public rclcpp::Node
{
public:
  ContactSensorSimulator()
  : Node("contact_sensor_simulator")
  {
    // SensorDataQoS는 센서의 오래된 backlog보다 최신 표본을 우선하는 best-effort/volatile preset이다.
    // 실제 제품에서는 네트워크 손실 허용도와 deadline을 측정한 뒤 QoS를 결정해야 한다.
    const auto sensor_qos = rclcpp::SensorDataQoS();
    joint_pub_ = create_publisher<sensor_msgs::msg::JointState>("/leg/joint_states", sensor_qos);
    wrench_pub_ = create_publisher<geometry_msgs::msg::WrenchStamped>("/foot/wrench", sensor_qos);
    imu_pub_ = create_publisher<sensor_msgs::msg::Imu>("/imu/data", sensor_qos);
    sample_pub_ = create_publisher<daily_robotics_2026_10_21::msg::ContactSample>(
      "/contact/sample", sensor_qos);

    // truth와 estimate는 감사 노드가 같은 stamp로 짝지어야 하므로 reliable depth 20을 사용한다.
    truth_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      "/sim/truth_pose", rclcpp::QoS(rclcpp::KeepLast(20)).reliable());

    // 5 ms wall timer는 200 Hz 센서 주기를 모사한다. wall timer는 ROS simulated time과 무관하다.
    timer_ = create_wall_timer(5ms, std::bind(&ContactSensorSimulator::on_timer, this));
  }

private:
  void on_timer()
  {
    // tick 기반 시각은 OS timer jitter와 무관한 재현 가능한 물리 궤적을 만든다.
    const double time_s = static_cast<double>(tick_) * kDtS;
    const VerticalTruth truth = make_truth(time_s);
    const rclcpp::Time stamp = get_clock()->now();

    // 한 주기의 앞 0.62 s는 stance, 나머지는 swing으로 두어 접촉 전이를 반복 관찰한다.
    const double gait_phase_s = std::fmod(time_s, 1.0);
    const bool in_contact = gait_phase_s < 0.62;
    const double normal_force_n = in_contact ?
      165.0 + 8.0 * std::sin(2.0 * kPi * time_s) :
      5.0 + 1.5 * std::sin(5.0 * kPi * time_s);

    // 평면 한 링크 다리에서 몸체 높이는 z=L cos(q), 속도는 zdot=-L sin(q) qdot이다.
    const double cosine_q = std::clamp(truth.height_m / kLegLengthM, -1.0, 1.0);
    double joint_position_rad = std::acos(cosine_q);
    const double sine_q = std::max(1.0e-6, std::sin(joint_position_rad));
    const double joint_velocity_rad_s =
      -truth.velocity_mps / (kLegLengthM * sine_q);

    // 1.50~1.60 s에 encoder/kinematic slip을 의도적으로 넣어 NIS gate가 동작하는지 검증한다.
    if (tick_ >= 300U && tick_ < 320U) {
      joint_position_rad += 0.28;
    }

    // 가속도계의 작은 고정 bias와 결정론적 잡음을 넣는다. EKF의 세 번째 state가 bias를 추정한다.
    constexpr double accel_bias_mps2 = 0.055;
    const double accel_noise_mps2 = 0.012 * std::sin(17.0 * time_s);
    const double gravity_compensated_accel_mps2 =
      truth.acceleration_mps2 + accel_bias_mps2 + accel_noise_mps2;

    geometry_msgs::msg::PoseStamped truth_message;
    truth_message.header.stamp = stamp;
    truth_message.header.frame_id = "map";
    truth_message.pose.position.z = truth.height_m;
    truth_message.pose.orientation.w = 1.0;
    truth_pub_->publish(truth_message);

    sensor_msgs::msg::JointState joint_message;
    joint_message.header.stamp = stamp;
    // JointState의 name/position/velocity 같은 index가 반드시 같은 관절을 뜻한다.
    joint_message.name = {"leg_pitch_joint"};
    joint_message.position = {joint_position_rad};
    joint_message.velocity = {joint_velocity_rad_s};
    joint_message.effort = {0.0};
    joint_pub_->publish(joint_message);

    geometry_msgs::msg::WrenchStamped wrench_message;
    wrench_message.header.stamp = stamp;
    // Wrench는 이 frame에서 표현된다. 여기서는 foot_sensor의 +z가 지면 법선 방향이다.
    wrench_message.header.frame_id = "foot_sensor";
    wrench_message.wrench.force.z = normal_force_n;
    wrench_pub_->publish(wrench_message);

    sensor_msgs::msg::Imu imu_message;
    imu_message.header.stamp = stamp;
    imu_message.header.frame_id = "imu_link";
    // orientation_covariance[0]=-1은 이 메시지가 orientation estimate를 제공하지 않음을 뜻한다.
    imu_message.orientation_covariance[0] = -1.0;
    // 정지한 +z-up IMU는 중력을 지탱하는 specific force +g를 읽는다.
    imu_message.linear_acceleration.z = gravity_compensated_accel_mps2 + kGravity;
    imu_message.linear_acceleration_covariance[8] = 0.02 * 0.02;
    imu_pub_->publish(imu_message);

    daily_robotics_2026_10_21::msg::ContactSample sample;
    sample.header.stamp = stamp;
    // 이 custom boundary의 frame은 중력 제거 후 수직축이 표현된 base_link다.
    sample.header.frame_id = "base_link";
    sample.joint_position_rad = joint_position_rad;
    sample.joint_velocity_rad_s = joint_velocity_rad_s;
    sample.gravity_compensated_accel_z_mps2 = gravity_compensated_accel_mps2;
    sample.normal_force_n = normal_force_n;
    sample_pub_->publish(sample);

    ++tick_;
  }

  std::uint32_t tick_{0U};
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_pub_;
  rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr wrench_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::Publisher<daily_robotics_2026_10_21::msg::ContactSample>::SharedPtr sample_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr truth_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 ROS argument, DDS context, signal handler를 초기화한다.
  rclcpp::init(argc, argv);
  // 시뮬레이터 callback은 하나뿐이므로 SingleThreaded spin으로 충분하다.
  rclcpp::spin(std::make_shared<ContactSensorSimulator>());
  rclcpp::shutdown();
  return 0;
}
