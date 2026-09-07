#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <string>

#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kRoomMinX = -2.5;
constexpr double kRoomMaxX = 2.5;
constexpr double kRoomMinY = -2.0;
constexpr double kRoomMaxY = 2.0;
constexpr double kWheelRadius = 0.08;
constexpr double kWheelSeparation = 0.42;
constexpr double kRangeSigma = 0.08;
constexpr std::size_t kParticleCount = 200;

double normalize_angle(double angle)
{
  return std::atan2(std::sin(angle), std::cos(angle));
}

double ray_to_room_wall(double x, double y, double angle)
{
  // sensor simulator와 같은 직사각형 지도 measurement model을 사용한다.
  const double direction_x = std::cos(angle);
  const double direction_y = std::sin(angle);
  double nearest = std::numeric_limits<double>::infinity();

  if (std::abs(direction_x) > 1.0e-9) {
    const double wall_x = direction_x > 0.0 ? kRoomMaxX : kRoomMinX;
    const double t = (wall_x - x) / direction_x;
    const double hit_y = y + t * direction_y;
    if (t > 0.0 && hit_y >= kRoomMinY && hit_y <= kRoomMaxY) {
      nearest = std::min(nearest, t);
    }
  }
  if (std::abs(direction_y) > 1.0e-9) {
    const double wall_y = direction_y > 0.0 ? kRoomMaxY : kRoomMinY;
    const double t = (wall_y - y) / direction_y;
    const double hit_x = x + t * direction_x;
    if (t > 0.0 && hit_x >= kRoomMinX && hit_x <= kRoomMaxX) {
      nearest = std::min(nearest, t);
    }
  }
  return nearest;
}

struct Particle
{
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
  double weight{1.0 / static_cast<double>(kParticleCount)};
};
}  // namespace

class ParticleLocalizer : public rclcpp::Node
{
public:
  ParticleLocalizer()
  : Node("particle_localizer"), random_engine_(20260908U)
  {
    // 고정 seed는 매 실행에서 같은 학습 결과를 만들어 디버깅과 회귀 비교를 쉽게 한다.
    std::uniform_real_distribution<double> x_distribution(kRoomMinX + 0.1, kRoomMaxX - 0.1);
    std::uniform_real_distribution<double> y_distribution(kRoomMinY + 0.1, kRoomMaxY - 0.1);
    std::uniform_real_distribution<double> yaw_distribution(-kPi, kPi);
    for (Particle & particle : particles_) {
      particle.x = x_distribution(random_engine_);
      particle.y = y_distribution(random_engine_);
      particle.yaw = yaw_distribution(random_engine_);
      particle.weight = 1.0 / static_cast<double>(kParticleCount);
    }

    pose_publisher_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/mcl_pose", rclcpp::QoS(10).reliable());
    // SensorDataQoS는 센서 생산자가 더 빠를 때 오래된 샘플보다 최신 샘플을 우선한다.
    wheel_subscription_ = create_subscription<sensor_msgs::msg::JointState>(
      "/wheel_states", rclcpp::SensorDataQoS(),
      std::bind(&ParticleLocalizer::on_wheel_state, this, std::placeholders::_1));
    scan_subscription_ = create_subscription<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SensorDataQoS(),
      std::bind(&ParticleLocalizer::on_scan, this, std::placeholders::_1));
  }

private:
  void on_wheel_state(const sensor_msgs::msg::JointState::SharedPtr message)
  {
    // 이 callback은 엔코더 누적각을 body frame의 이동량/회전량으로 바꾸고 모든 입자를 예측한다.
    if (message->name.size() < 2U || message->position.size() < 2U) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000, "wheel state has fewer than two joints");
      return;
    }

    std::size_t left_index = message->name.size();
    std::size_t right_index = message->name.size();
    for (std::size_t index = 0; index < message->name.size(); ++index) {
      if (message->name[index] == "left_wheel_joint") {
        left_index = index;
      } else if (message->name[index] == "right_wheel_joint") {
        right_index = index;
      }
    }
    if (left_index >= message->position.size() || right_index >= message->position.size()) {
      return;
    }

    const double current_left = message->position[left_index];
    const double current_right = message->position[right_index];
    if (!have_previous_wheels_) {
      previous_left_ = current_left;
      previous_right_ = current_right;
      have_previous_wheels_ = true;
      return;
    }

    // 바퀴 회전각 변화(rad)에 반지름을 곱하면 지면에서 이동한 호 길이(m)가 된다.
    const double distance_left = (current_left - previous_left_) * kWheelRadius;
    const double distance_right = (current_right - previous_right_) * kWheelRadius;
    previous_left_ = current_left;
    previous_right_ = current_right;

    // 차동구동 순운동학: ds=(dr+dl)/2, dtheta=(dr-dl)/L.
    const double distance = 0.5 * (distance_right + distance_left);
    const double yaw_change = (distance_right - distance_left) / kWheelSeparation;
    predict(distance, yaw_change);
  }

  void predict(double distance, double yaw_change)
  {
    // 실제 wheel slip/encoder quantization을 흉내 내어 이동량에 비례하는 motion noise를 더한다.
    const double translation_sigma = 0.002 + 0.025 * std::abs(distance);
    const double rotation_sigma = 0.001 + 0.025 * std::abs(yaw_change);
    for (Particle & particle : particles_) {
      const double noisy_distance =
        distance + translation_sigma * unit_normal_(random_engine_);
      const double noisy_yaw_change =
        yaw_change + rotation_sigma * unit_normal_(random_engine_);
      // 회전 중 이동을 midpoint heading으로 적분하면 Euler 전진보다 원호 오차가 작다.
      const double midpoint_yaw = particle.yaw + 0.5 * noisy_yaw_change;
      particle.x += noisy_distance * std::cos(midpoint_yaw);
      particle.y += noisy_distance * std::sin(midpoint_yaw);
      particle.yaw = normalize_angle(particle.yaw + noisy_yaw_change);
      // 이 데모는 벽 밖 상태를 허용하지 않는 알려진 지도 localization이므로 수치 잡음만큼의 이탈을 clamp한다.
      particle.x = std::clamp(particle.x, kRoomMinX + 0.01, kRoomMaxX - 0.01);
      particle.y = std::clamp(particle.y, kRoomMinY + 0.01, kRoomMaxY - 0.01);
    }
  }

  void on_scan(const sensor_msgs::msg::LaserScan::SharedPtr scan)
  {
    // 이 callback은 각 입자가 예측한 벽 거리와 실제 scan의 차이로 importance weight를 갱신한다.
    if (scan->ranges.empty()) {
      return;
    }

    std::array<double, kParticleCount> log_weights{};
    double maximum_log_weight = -std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < kParticleCount; ++index) {
      const Particle & particle = particles_[index];
      double log_weight = std::log(std::max(particle.weight, 1.0e-300));

      for (std::size_t beam = 0; beam < scan->ranges.size(); ++beam) {
        const double measured_range = scan->ranges[beam];
        if (!std::isfinite(measured_range) || measured_range < scan->range_min ||
          measured_range > scan->range_max)
        {
          continue;
        }
        const double beam_angle = scan->angle_min + beam * scan->angle_increment;
        const double predicted_range = ray_to_room_wall(
          particle.x, particle.y, particle.yaw + beam_angle);
        const double residual = measured_range - predicted_range;
        // Gaussian log-likelihood: log p(z|x) = -0.5*(z-h(x))^2/sigma^2 + 상수.
        log_weight += -0.5 * residual * residual / (kRangeSigma * kRangeSigma);
      }
      log_weights[index] = log_weight;
      maximum_log_weight = std::max(maximum_log_weight, log_weight);
    }

    // log-sum-exp trick: 가장 큰 log weight를 빼고 exp해 underflow를 줄인다.
    double weight_sum = 0.0;
    for (std::size_t index = 0; index < kParticleCount; ++index) {
      particles_[index].weight = std::exp(log_weights[index] - maximum_log_weight);
      weight_sum += particles_[index].weight;
    }
    if (!std::isfinite(weight_sum) || weight_sum <= 1.0e-12) {
      reset_uniform_weights();
    } else {
      for (Particle & particle : particles_) {
        particle.weight /= weight_sum;
      }
    }

    // N_eff=1/sum(w_i^2). 작을수록 소수 입자에 확률이 붕괴했다는 뜻이다.
    double squared_weight_sum = 0.0;
    for (const Particle & particle : particles_) {
      squared_weight_sum += particle.weight * particle.weight;
    }
    const double effective_count = 1.0 / squared_weight_sum;
    if (effective_count < 0.5 * static_cast<double>(kParticleCount)) {
      systematic_resample();
    }

    publish_estimate(effective_count);
  }

  void reset_uniform_weights()
  {
    for (Particle & particle : particles_) {
      particle.weight = 1.0 / static_cast<double>(kParticleCount);
    }
  }

  void systematic_resample()
  {
    // 하나의 난수 u0와 등간격 u_m=u0+m/N을 사용해 multinomial보다 분산이 낮게 재표본화한다.
    std::array<Particle, kParticleCount> resampled{};
    std::uniform_real_distribution<double> offset_distribution(
      0.0, 1.0 / static_cast<double>(kParticleCount));
    const double initial_offset = offset_distribution(random_engine_);
    std::size_t source_index = 0U;
    double cumulative_weight = particles_[0].weight;

    for (std::size_t output_index = 0; output_index < kParticleCount; ++output_index) {
      const double threshold =
        initial_offset + static_cast<double>(output_index) / static_cast<double>(kParticleCount);
      while (threshold > cumulative_weight && source_index + 1U < kParticleCount) {
        ++source_index;
        cumulative_weight += particles_[source_index].weight;
      }
      resampled[output_index] = particles_[source_index];
      resampled[output_index].weight = 1.0 / static_cast<double>(kParticleCount);
    }
    particles_ = resampled;
  }

  void publish_estimate(double effective_count)
  {
    // yaw는 선형 평균 대신 atan2(sum w sin(theta), sum w cos(theta))로 원형 평균한다.
    double mean_x = 0.0;
    double mean_y = 0.0;
    double weighted_sine = 0.0;
    double weighted_cosine = 0.0;
    for (const Particle & particle : particles_) {
      mean_x += particle.weight * particle.x;
      mean_y += particle.weight * particle.y;
      weighted_sine += particle.weight * std::sin(particle.yaw);
      weighted_cosine += particle.weight * std::cos(particle.yaw);
    }
    const double mean_yaw = std::atan2(weighted_sine, weighted_cosine);

    double variance_x = 0.0;
    double variance_y = 0.0;
    double variance_yaw = 0.0;
    double covariance_xy = 0.0;
    for (const Particle & particle : particles_) {
      const double error_x = particle.x - mean_x;
      const double error_y = particle.y - mean_y;
      const double error_yaw = normalize_angle(particle.yaw - mean_yaw);
      variance_x += particle.weight * error_x * error_x;
      variance_y += particle.weight * error_y * error_y;
      variance_yaw += particle.weight * error_yaw * error_yaw;
      covariance_xy += particle.weight * error_x * error_y;
    }

    geometry_msgs::msg::PoseWithCovarianceStamped output;
    output.header.stamp = now();
    output.header.frame_id = "map";
    output.pose.pose.position.x = mean_x;
    output.pose.pose.position.y = mean_y;
    // 평면 yaw quaternion: q=[0,0,sin(yaw/2),cos(yaw/2)].
    output.pose.pose.orientation.z = std::sin(0.5 * mean_yaw);
    output.pose.pose.orientation.w = std::cos(0.5 * mean_yaw);
    // Pose covariance는 [x,y,z,roll,pitch,yaw] 순서의 6x6 row-major 배열이다.
    output.pose.covariance[0] = variance_x;
    output.pose.covariance[1] = covariance_xy;
    output.pose.covariance[6] = covariance_xy;
    output.pose.covariance[7] = variance_y;
    output.pose.covariance[35] = variance_yaw;
    pose_publisher_->publish(output);

    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 1000,
      "MCL: x=%.3f y=%.3f yaw=%.3f N_eff=%.1f/%zu",
      mean_x, mean_y, mean_yaw, effective_count, kParticleCount);
  }

  std::array<Particle, kParticleCount> particles_{};
  std::minstd_rand random_engine_;
  std::normal_distribution<double> unit_normal_{0.0, 1.0};

  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pose_publisher_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr wheel_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_subscription_;
  bool have_previous_wheels_{false};
  double previous_left_{0.0};
  double previous_right_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // 단일 executor가 wheel predict와 scan update를 직렬화해 particle 배열에 별도 mutex가 필요 없다.
  rclcpp::spin(std::make_shared<ParticleLocalizer>());
  rclcpp::shutdown();
  return 0;
}
