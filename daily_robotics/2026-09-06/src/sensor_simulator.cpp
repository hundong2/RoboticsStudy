#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

// 이 노드는 실제 바퀴 엔코더/자이로/GNSS 드라이버를 대신한다. EKF가 예측에 쓸 속도와
// 보정에 쓸 전역 위치를 게시하며, 통신 감시 실습을 위해 wheel 스트림 장애도 주입한다.
class SensorSimulator final : public rclcpp::Node
{
public:
  SensorSimulator()
  : Node("sensor_simulator")
  {
    // KeepLast(5)는 소비자가 잠깐 늦어져도 최근 5개만 보관해 오래된 제어 입력의 누적을 막는다.
    // Deadline 100 ms는 50 Hz(20 ms) 스트림에 여유를 둔 최대 도착 간격 계약이다.
    // MANUAL_BY_TOPIC은 이 Publisher가 직접 assert_liveliness()를 호출해야 살아 있다고 본다.
    rclcpp::QoS wheel_qos(rclcpp::KeepLast(5));
    wheel_qos.reliable()
      .deadline(100ms)
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(250ms);

    // PublisherEventCallbacks는 구독 메시지 콜백과 별개로 DDS QoS 계약 위반을 알려준다.
    rclcpp::PublisherOptions wheel_options;
    wheel_options.event_callbacks.deadline_callback =
      [this](rclcpp::QOSDeadlineOfferedInfo & info) {
        RCLCPP_WARN(
          get_logger(), "[publisher] offered deadline missed: +%d (total=%d)",
          info.total_count_change, info.total_count);
      };
    wheel_options.event_callbacks.liveliness_callback =
      [this](rclcpp::QOSLivelinessLostInfo & info) {
        RCLCPP_WARN(
          get_logger(), "[publisher] liveliness lost: +%d (total=%d)",
          info.total_count_change, info.total_count);
      };

    // TwistStamped.linear.x는 전진 속도[m/s], angular.z는 yaw 각속도[rad/s]로 사용한다.
    wheel_publisher_ = create_publisher<geometry_msgs::msg::TwistStamped>(
      "/wheel/twist", wheel_qos, wheel_options);

    // GPS는 저주기 절대 위치이므로 신뢰 전송과 작은 큐를 사용한다.
    gps_publisher_ = create_publisher<geometry_msgs::msg::PointStamped>(
      "/gps/position", rclcpp::QoS(rclcpp::KeepLast(3)).reliable());

    // create_wall_timer는 ROS 시간 정지와 무관한 wall clock으로 20 ms마다 센서를 갱신한다.
    timer_ = create_wall_timer(20ms, std::bind(&SensorSimulator::on_timer, this));

    RCLCPP_INFO(
      get_logger(),
      "50 Hz wheel + 5 Hz GPS started; wheel pauses for 600 ms every 5 s");
  }

private:
  // 센서 모델을 한 스텝 전진시키고 ROS 메시지를 게시하는 50 Hz 타이머 콜백이다.
  void on_timer()
  {
    constexpr double dt = 0.02;
    time_s_ += dt;

    // 부드럽게 변하는 참 속도. 실제 장비에서는 모터/IMU 드라이버가 이 값을 측정한다.
    const double true_v = 0.60 + 0.10 * std::sin(0.40 * time_s_);
    const double true_omega = 0.25 * std::sin(0.20 * time_s_);

    // 차동구동 로봇의 unicycle 운동학:
    // x' = x + v cos(yaw) dt, y' = y + v sin(yaw) dt, yaw' = yaw + omega dt.
    true_x_ += true_v * std::cos(true_yaw_) * dt;
    true_y_ += true_v * std::sin(true_yaw_) * dt;
    true_yaw_ += true_omega * dt;

    // 매 5초의 [3.0, 3.6)초 구간에는 wheel 게시와 liveliness assertion을 모두 멈춘다.
    // 따라서 100 ms Deadline과 250 ms Liveliness가 서로 다른 시간 의미로 장애를 잡는다.
    const double phase = std::fmod(time_s_, 5.0);
    const bool inject_dropout = phase >= 3.0 && phase < 3.6;
    if (inject_dropout != dropout_active_) {
      dropout_active_ = inject_dropout;
      RCLCPP_WARN(
        get_logger(), "wheel dropout %s", dropout_active_ ? "START" : "END");
    }

    if (!inject_dropout) {
      geometry_msgs::msg::TwistStamped wheel_message;
      // header.stamp는 수신 시각이 아니라 이 측정이 생성된 시각을 전달한다.
      wheel_message.header.stamp = now();
      wheel_message.header.frame_id = "base_link";
      // 난수 대신 결정론적 사인 잡음을 써서 매 실행 결과를 비교 가능하게 만든다.
      wheel_message.twist.linear.x = true_v + 0.02 * std::sin(13.0 * time_s_);
      wheel_message.twist.angular.z = true_omega + 0.01 * std::cos(7.0 * time_s_);
      wheel_publisher_->publish(wheel_message);

      // MANUAL_BY_TOPIC lease를 갱신한다. 게시와 별도 API라서 장치 heartbeat에도 쓸 수 있다.
      if (!wheel_publisher_->assert_liveliness()) {
        RCLCPP_WARN(get_logger(), "assert_liveliness() was not accepted by the RMW");
      }
    }

    // 50 Hz 타이머의 10회마다 1번 게시하므로 GPS 주기는 5 Hz다.
    ++tick_count_;
    if (tick_count_ % 10U == 0U) {
      geometry_msgs::msg::PointStamped gps_message;
      gps_message.header.stamp = now();
      gps_message.header.frame_id = "map";
      gps_message.point.x = true_x_ + 0.20 * std::sin(2.30 * time_s_);
      gps_message.point.y = true_y_ + 0.20 * std::cos(1.70 * time_s_);
      gps_message.point.z = 0.0;
      gps_publisher_->publish(gps_message);
    }
  }

  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr wheel_publisher_;
  rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr gps_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;

  double time_s_{0.0};
  double true_x_{0.0};
  double true_y_{0.0};
  double true_yaw_{0.0};
  std::uint64_t tick_count_{0U};
  bool dropout_active_{false};
};

int main(int argc, char * argv[])
{
  // rclcpp::init은 DDS/RMW 통신 계층과 ROS 인자 처리를 초기화한다.
  rclcpp::init(argc, argv);
  // spin은 Timer와 QoS 이벤트 콜백을 준비되는 순서대로 실행한다.
  rclcpp::spin(std::make_shared<SensorSimulator>());
  rclcpp::shutdown();
  return 0;
}
