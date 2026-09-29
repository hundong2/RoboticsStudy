#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "daily_robotics_2026_09_30/msg/contact_estimate.hpp"
#include "daily_robotics_2026_09_30/msg/safety_status.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "rclcpp/rclcpp.hpp"

// FrictionFallbackSupervisor의 역할:
// 추정기의 마찰 하한을 독립적으로 검사하고, 위험이 연속 3회 보이면 접선 명령을 0으로 만든다.
// 추정기 내부 mode만 믿지 않고 age/slip/margin을 다시 검사하는 것이 독립 안전 계층의 핵심이다.
class FrictionFallbackSupervisor : public rclcpp::Node
{
public:
  FrictionFallbackSupervisor()
  : Node("friction_fallback_supervisor")
  {
    request_subscription_ = create_subscription<geometry_msgs::msg::WrenchStamped>(
      "/controller/requested_wrench", rclcpp::QoS(10).reliable(),
      [this](const geometry_msgs::msg::WrenchStamped::ConstSharedPtr message) {
        latest_request_ = *message;
        have_request_ = true;
      });
    estimate_subscription_ =
      create_subscription<daily_robotics_2026_09_30::msg::ContactEstimate>(
      "/contact/estimate", rclcpp::QoS(10).reliable(),
      [this](const daily_robotics_2026_09_30::msg::ContactEstimate::ConstSharedPtr message) {
        supervise(message);
      });

    safe_wrench_publisher_ = create_publisher<geometry_msgs::msg::WrenchStamped>(
      "/controller/safe_wrench", rclcpp::QoS(10).reliable());
    status_publisher_ = create_publisher<daily_robotics_2026_09_30::msg::SafetyStatus>(
      "/safety/status", rclcpp::QoS(10).reliable());
  }

private:
  static constexpr std::uint8_t kDisarmed = 0U;
  static constexpr std::uint8_t kActive = 1U;
  static constexpr std::uint8_t kFallback = 2U;

  void supervise(const daily_robotics_2026_09_30::msg::ContactEstimate::ConstSharedPtr & estimate)
  {
    // header.stamp는 센서 측정 시각이다. 지금과의 차이는 DDS/Executor 지연까지 포함한 sample age다.
    const double estimate_age_ms =
      (now() - rclcpp::Time(estimate->header.stamp)).seconds() * 1000.0;
    const bool contact_lost = estimate->contact_probability < 0.65;
    const bool unsafe =
      estimate->mode == 2U || estimate->slip_speed_mps > 0.025 ||
      estimate->friction_margin_n < 5.0 || estimate_age_ms > 20.0;
    const bool good = !contact_lost && !unsafe;

    consecutive_good_ = good ? std::min(consecutive_good_ + 1U, 100000U) : 0U;
    consecutive_bad_ = unsafe ? std::min(consecutive_bad_ + 1U, 100000U) : 0U;

    if (contact_lost) {
      // 공중 상태에서는 우연히 남은 이전 추정값으로 힘을 내지 않도록 즉시 disarm한다.
      state_ = kDisarmed;
      consecutive_good_ = 0U;
    } else if (state_ == kDisarmed && consecutive_good_ >= 50U) {
      // 500 Hz에서 50표본=100 ms. 접촉이 안정적으로 지속된 뒤에만 actuation을 허용한다.
      state_ = kActive;
    } else if (state_ == kActive && consecutive_bad_ >= 3U) {
      // 3표본=6 ms debounce는 단일 센서 spike에는 버티되 slip에는 빠르게 반응하는 교육용 값이다.
      state_ = kFallback;
      fallback_release_time_ = now() + rclcpp::Duration::from_seconds(0.4);
      ++trip_count_;
      RCLCPP_WARN(
        get_logger(), "FALLBACK trip=%u mode=%u slip=%.4f margin=%.2f age_ms=%.2f",
        trip_count_, estimate->mode, estimate->slip_speed_mps,
        estimate->friction_margin_n, estimate_age_ms);
    } else if (
      state_ == kFallback && now() >= fallback_release_time_ && consecutive_good_ >= 80U)
    {
      // 최소 400 ms hold와 160 ms 연속 정상 표본을 모두 만족해야 자동 복귀한다.
      state_ = kActive;
      RCLCPP_INFO(get_logger(), "RECOVERY trip=%u after stable hysteresis", trip_count_);
    }

    const double requested_force = have_request_ ? latest_request_.wrench.force.x : 0.0;
    // limit = mu_lower*F_n-5N은 Coulomb friction cone에서 5 N의 절대 여유를 뺀 값이다.
    const double conservative_limit = std::max(
      0.0, estimate->friction_lower_bound * estimate->normal_force_n - 5.0);
    double allowed_force = 0.0;
    if (state_ == kActive) {
      allowed_force = std::clamp(requested_force, -conservative_limit, conservative_limit);
    }

    geometry_msgs::msg::WrenchStamped safe_wrench;
    safe_wrench.header = estimate->header;
    safe_wrench.wrench.force.x = allowed_force;
    safe_wrench_publisher_->publish(safe_wrench);

    daily_robotics_2026_09_30::msg::SafetyStatus status;
    status.header = estimate->header;
    status.state = state_;
    status.requested_force_n = requested_force;
    status.allowed_force_n = allowed_force;
    status.conservative_limit_n = conservative_limit;
    status.estimate_age_ms = estimate_age_ms;
    status.consecutive_good = consecutive_good_;
    status.consecutive_bad = consecutive_bad_;
    status.trip_count = trip_count_;
    status_publisher_->publish(status);
  }

  std::uint8_t state_{kDisarmed};
  std::uint32_t consecutive_good_{0U};
  std::uint32_t consecutive_bad_{0U};
  std::uint32_t trip_count_{0U};
  bool have_request_{false};
  rclcpp::Time fallback_release_time_{0, 0, RCL_ROS_TIME};
  geometry_msgs::msg::WrenchStamped latest_request_{};
  rclcpp::Subscription<geometry_msgs::msg::WrenchStamped>::SharedPtr request_subscription_;
  rclcpp::Subscription<daily_robotics_2026_09_30::msg::ContactEstimate>::SharedPtr
    estimate_subscription_;
  rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr safe_wrench_publisher_;
  rclcpp::Publisher<daily_robotics_2026_09_30::msg::SafetyStatus>::SharedPtr status_publisher_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // 단일 스레드 spin 덕분에 request 갱신과 supervise가 동시에 멤버를 수정하지 않는다.
  rclcpp::spin(std::make_shared<FrictionFallbackSupervisor>());
  rclcpp::shutdown();
  return 0;
}
