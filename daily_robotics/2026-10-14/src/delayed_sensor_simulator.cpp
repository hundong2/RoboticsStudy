#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace
{
// 센서 생성 주기다. wall timer는 callback을 깨우는 용도이고, 추정기는 실제 Header.stamp 차이를 쓴다.
constexpr auto kSensorPeriod = 20ms;
constexpr std::uint64_t kVisionStride = 5U;  // 50 Hz 중 매 5번째이므로 vision은 10 Hz다.
constexpr std::uint64_t kDelayTicks = 8U;    // 약 8 * 20 ms = 160 ms 뒤에 위치를 보낸다.
constexpr std::uint64_t kVisionStartTick = 50U;  // DDS discovery/history가 준비될 1초를 확보한다.
constexpr std::size_t kPendingCapacity = 16U;

double truth_velocity(const double t)
{
  // v(t) = 1 + 0.6 sin(0.8t) + 0.3 sin(2.1t). 여러 주파수의 가속으로 시간 지연 효과를 드러낸다.
  return 1.0 + 0.6 * std::sin(0.8 * t) + 0.3 * std::sin(2.1 * t);
}

double truth_position(const double t)
{
  // 위 속도를 0부터 t까지 적분한 해석해다.
  // p(t) = t + 0.6/0.8(1-cos(0.8t)) + 0.3/2.1(1-cos(2.1t)).
  return t + (0.6 / 0.8) * (1.0 - std::cos(0.8 * t)) +
         (0.3 / 2.1) * (1.0 - std::cos(2.1 * t));
}
}  // namespace

class DelayedSensorSimulator final : public rclcpp::Node
{
public:
  DelayedSensorSimulator()
  : Node("delayed_sensor_simulator"), start_stamp_(this->now())
  {
    // SensorDataQoS는 sensor stream에서 최신값을 우선하는 best-effort/volatile profile이다.
    // keep_last(8)은 middleware queue가 무한히 자라지 않도록 깊이를 명시한다.
    const auto sensor_qos = rclcpp::SensorDataQoS().keep_last(8);
    velocity_pub_ = create_publisher<geometry_msgs::msg::TwistStamped>(
      "/study/wheel_velocity", sensor_qos);
    delayed_position_pub_ = create_publisher<geometry_msgs::msg::PointStamped>(
      "/study/delayed_position", sensor_qos);
    truth_pub_ = create_publisher<nav_msgs::msg::Odometry>(
      "/study/ground_truth", sensor_qos);

    // create_wall_timer는 steady wall clock 기반으로 callback을 예약한다.
    // 메시지의 측정 시각은 callback 도착 시각과 분리하여 ROS clock의 now()로 기록한다.
    timer_ = create_wall_timer(kSensorPeriod, std::bind(&DelayedSensorSimulator::on_timer, this));

    RCLCPP_INFO(
      get_logger(),
      "50 Hz velocity/truth + 10 Hz position with about 160 ms delivery delay");
  }

private:
  struct PendingPosition
  {
    geometry_msgs::msg::PointStamped message{};
    std::uint64_t release_tick{0U};
    bool valid{false};
  };

  // 이 callback은 로봇의 실제 운동, 즉시 도착하는 wheel 속도, 늦게 도착하는 위치 센서를 만든다.
  void on_timer()
  {
    const rclcpp::Time stamp = this->now();
    const double t = (stamp - start_stamp_).seconds();
    const double position = truth_position(t);
    const double velocity = truth_velocity(t);

    geometry_msgs::msg::TwistStamped wheel;
    // Header.stamp는 publish 시각이 아니라 이 속도 sample의 측정 시각이라는 계약이다.
    wheel.header.stamp = stamp;
    wheel.header.frame_id = "base_link";
    // 결정론적인 작은 ripple을 넣어 filter가 실제 measurement update를 수행하게 한다.
    wheel.twist.linear.x = velocity + 0.025 * std::sin(13.0 * t);
    velocity_pub_->publish(wheel);

    nav_msgs::msg::Odometry truth;
    truth.header.stamp = stamp;
    truth.header.frame_id = "map";
    truth.child_frame_id = "base_link";
    truth.pose.pose.position.x = position;
    truth.pose.pose.orientation.w = 1.0;
    truth.twist.twist.linear.x = velocity;
    truth_pub_->publish(truth);

    if (tick_ >= kVisionStartTick && (tick_ % kVisionStride) == 0U) {
      enqueue_delayed_position(stamp, t, position);
    }
    release_due_positions();
    ++tick_;
  }

  // 위치 측정값은 지금 만들되 Header.stamp를 보존한 채 약 160 ms 뒤에 publish할 queue에 넣는다.
  void enqueue_delayed_position(
    const rclcpp::Time & measurement_stamp, const double t, const double position)
  {
    for (auto & slot : pending_positions_) {
      if (!slot.valid) {
        slot.message.header.stamp = measurement_stamp;
        slot.message.header.frame_id = "map";
        // sine 기반 결정론적 noise는 실행마다 같은 입력을 만들어 regression test를 재현 가능하게 한다.
        slot.message.point.x = position + 0.035 * std::sin(17.0 * t + 0.4);
        slot.message.point.y = 0.0;
        slot.message.point.z = 0.0;
        slot.release_tick = tick_ + kDelayTicks;
        slot.valid = true;
        return;
      }
    }

    // 정상 설정에서는 최대 두 건 정도만 대기하므로 이 경로는 sensor queue 설계 오류를 드러낸다.
    RCLCPP_ERROR_THROTTLE(
      get_logger(), *get_clock(), 1000, "pending vision buffer overflow");
  }

  // release tick에 도달한 과거 위치를 publish한다. 도착은 늦지만 message stamp는 과거 그대로다.
  void release_due_positions()
  {
    for (auto & slot : pending_positions_) {
      if (slot.valid && slot.release_tick <= tick_) {
        delayed_position_pub_->publish(slot.message);
        slot.valid = false;
      }
    }
  }

  rclcpp::Time start_stamp_;
  std::uint64_t tick_{0U};
  std::array<PendingPosition, kPendingCapacity> pending_positions_{};
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr velocity_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr delayed_position_pub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr truth_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  // init은 DDS, signal handler, ROS argument를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 executor가 subscription/timer wait-set을 감시하고 준비된 callback을 실행하게 한다.
  // 기본 SingleThreadedExecutor이므로 이 노드의 queue와 timer는 동시에 수정되지 않는다.
  rclcpp::spin(std::make_shared<DelayedSensorSimulator>());
  rclcpp::shutdown();
  return 0;
}
