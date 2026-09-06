#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "geometry_msgs/msg/twist_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rcl_interfaces/msg/set_parameters_result.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

using namespace std::chrono_literals;

namespace daily_robotics_2026_09_07
{

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr std::size_t kMaxObstacles = 360U;
constexpr std::size_t kLinearSamples = 7U;
constexpr std::size_t kAngularSamples = 15U;
constexpr std::size_t kMaxCandidates = kLinearSamples * kAngularSamples;
constexpr double kControlPeriodSeconds = 0.10;
constexpr double kSimulationStepSeconds = 0.10;

double normalize_angle(double angle)
{
  // atan2(sin, cos)는 임의의 각도를 [-pi, pi]로 감싼다.
  return std::atan2(std::sin(angle), std::cos(angle));
}

double interpolate(const double minimum, const double maximum, const std::size_t index,
  const std::size_t sample_count)
{
  if (sample_count <= 1U) {
    return minimum;
  }
  const double ratio = static_cast<double>(index) /
    static_cast<double>(sample_count - 1U);
  return minimum + ratio * (maximum - minimum);
}
}  // namespace

// 이 노드는 LaserScan과 현재 속도를 받아 DWA 후보를 제한된 개수만 평가하고,
// 충돌 전에 제동 가능한 최상 후보를 /cmd_vel_raw로 게시한다.
class DwaPlannerComponent final : public rclcpp::Node
{
public:
  explicit DwaPlannerComponent(const rclcpp::NodeOptions & options)
  : Node("dwa_planner", options)
  {
    // declare_parameter는 타입과 기본값을 ROS parameter service에 등록한다.
    // launch override가 있으면 반환값은 기본값 대신 override를 포함한다.
    goal_x_.store(declare_parameter<double>("goal_x", 3.0));
    goal_y_.store(declare_parameter<double>("goal_y", 0.8));
    max_velocity_.store(declare_parameter<double>("max_velocity", 0.80));
    max_angular_velocity_.store(
      declare_parameter<double>("max_angular_velocity", 1.20));
    max_acceleration_.store(declare_parameter<double>("max_acceleration", 0.80));
    max_angular_acceleration_.store(
      declare_parameter<double>("max_angular_acceleration", 1.80));
    robot_radius_.store(declare_parameter<double>("robot_radius", 0.25));
    simulation_horizon_.store(
      declare_parameter<double>("simulation_horizon", 1.50));
    heading_weight_.store(declare_parameter<double>("heading_weight", 0.45));
    clearance_weight_.store(declare_parameter<double>("clearance_weight", 0.35));
    speed_weight_.store(declare_parameter<double>("speed_weight", 0.20));

    // on-set callback은 값이 실제 저장되기 전에 호출된다. 여기서는 범위 검증만 수행하여
    // 거부된 parameter transaction이 planner 내부 상태를 먼저 바꾸는 일을 막는다.
    parameter_validation_handle_ = add_on_set_parameters_callback(
      std::bind(&DwaPlannerComponent::validate_parameters, this, std::placeholders::_1));
    // post-set callback은 모든 검증을 통과해 parameter가 저장된 뒤 호출된다.
    // 원자 변수 갱신은 planner timer가 mutex 대기 없이 일관된 스칼라 snapshot을 읽게 한다.
    parameter_commit_handle_ = add_post_set_parameters_callback(
      std::bind(&DwaPlannerComponent::commit_parameters, this, std::placeholders::_1));

    // SensorDataQoS는 센서의 최신성 우선 정책과 맞춘다. callback은 UniquePtr를 받아
    // intra-process 단일 구독자 경로에서 serialization과 message 복사를 피할 수 있다.
    scan_subscription_ = create_subscription<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SensorDataQoS(),
      std::bind(&DwaPlannerComponent::on_scan, this, std::placeholders::_1));
    odometry_subscription_ = create_subscription<nav_msgs::msg::Odometry>(
      "/odom", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
      std::bind(&DwaPlannerComponent::on_odometry, this, std::placeholders::_1));
    raw_command_publisher_ = create_publisher<geometry_msgs::msg::TwistStamped>(
      "/cmd_vel_raw", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());

    // planner callback은 10 Hz로만 실행된다. 기본값은 15 rollout step이고,
    // 허용된 최대 horizon에서도 7*15 candidates * 30 steps * 360 obstacles가 상한이다.
    planning_timer_ = create_wall_timer(
      100ms, std::bind(&DwaPlannerComponent::plan_once, this));

    RCLCPP_INFO(
      get_logger(),
      "DWA component ready: max_candidates=%zu, fixed_obstacles=%zu, intra-process=%s",
      kMaxCandidates, kMaxObstacles,
      options.use_intra_process_comms() ? "true" : "false");
  }

private:
  struct ObstaclePoint
  {
    double x{0.0};
    double y{0.0};
  };

  struct PlannerConfig
  {
    double goal_x;
    double goal_y;
    double max_velocity;
    double max_angular_velocity;
    double max_acceleration;
    double max_angular_acceleration;
    double robot_radius;
    double simulation_horizon;
    double heading_weight;
    double clearance_weight;
    double speed_weight;
  };

  struct Candidate
  {
    double velocity{0.0};
    double angular_velocity{0.0};
    double score{-std::numeric_limits<double>::infinity()};
    double minimum_clearance{0.0};
    bool admissible{false};
  };

  // LaserScan의 극좌표 (range, angle)를 로봇 좌표계의 장애물 점 (x,y)로 바꾼다.
  // x=r*cos(theta), y=r*sin(theta)이며 std::array에 써서 planner hot path의 heap 할당을 없앤다.
  void on_scan(sensor_msgs::msg::LaserScan::UniquePtr scan)
  {
    const void * const received_address = scan.get();
    obstacle_count_ = 0U;
    const std::size_t bounded_count = std::min(scan->ranges.size(), kMaxObstacles);

    for (std::size_t index = 0U; index < bounded_count; ++index) {
      const double range = static_cast<double>(scan->ranges[index]);
      if (!std::isfinite(range) || range < scan->range_min ||
        range >= static_cast<double>(scan->range_max) * 0.995)
      {
        continue;
      }

      const double angle = static_cast<double>(scan->angle_min) +
        static_cast<double>(index) * static_cast<double>(scan->angle_increment);
      obstacles_[obstacle_count_] = {
        range * std::cos(angle),
        range * std::sin(angle),
      };
      ++obstacle_count_;
    }

    have_scan_ = true;
    ++scan_count_;
    if (scan_count_ % 20U == 1U) {
      RCLCPP_INFO(
        get_logger(), "received scan unique_ptr=%p retained_obstacles=%zu",
        received_address, obstacle_count_);
    }
  }

  // Odometry twist를 DWA dynamic window의 중심 속도로 저장한다.
  void on_odometry(nav_msgs::msg::Odometry::UniquePtr odometry)
  {
    current_velocity_ = odometry->twist.twist.linear.x;
    current_angular_velocity_ = odometry->twist.twist.angular.z;
    have_odometry_ = true;
  }

  PlannerConfig load_config() const
  {
    // memory_order_relaxed는 스칼라 간 인과 순서가 필요 없고 최신 완결 값만 필요할 때 사용한다.
    return PlannerConfig{
      goal_x_.load(std::memory_order_relaxed),
      goal_y_.load(std::memory_order_relaxed),
      max_velocity_.load(std::memory_order_relaxed),
      max_angular_velocity_.load(std::memory_order_relaxed),
      max_acceleration_.load(std::memory_order_relaxed),
      max_angular_acceleration_.load(std::memory_order_relaxed),
      robot_radius_.load(std::memory_order_relaxed),
      simulation_horizon_.load(std::memory_order_relaxed),
      heading_weight_.load(std::memory_order_relaxed),
      clearance_weight_.load(std::memory_order_relaxed),
      speed_weight_.load(std::memory_order_relaxed),
    };
  }

  // 가속도 한계로 이번 100 ms 안에 도달 가능한 [v,w] 범위만 샘플링한다.
  std::size_t build_dynamic_window(const PlannerConfig & config)
  {
    const double velocity_min = std::clamp(
      current_velocity_ - config.max_acceleration * kControlPeriodSeconds,
      0.0, config.max_velocity);
    const double velocity_max = std::clamp(
      current_velocity_ + config.max_acceleration * kControlPeriodSeconds,
      0.0, config.max_velocity);
    const double angular_min = std::clamp(
      current_angular_velocity_ - config.max_angular_acceleration * kControlPeriodSeconds,
      -config.max_angular_velocity, config.max_angular_velocity);
    const double angular_max = std::clamp(
      current_angular_velocity_ + config.max_angular_acceleration * kControlPeriodSeconds,
      -config.max_angular_velocity, config.max_angular_velocity);

    std::size_t candidate_index = 0U;
    for (std::size_t linear_index = 0U; linear_index < kLinearSamples; ++linear_index) {
      for (std::size_t angular_index = 0U; angular_index < kAngularSamples; ++angular_index) {
        Candidate & candidate = candidates_[candidate_index];
        candidate.velocity = interpolate(
          velocity_min, velocity_max, linear_index, kLinearSamples);
        candidate.angular_velocity = interpolate(
          angular_min, angular_max, angular_index, kAngularSamples);
        candidate.score = -std::numeric_limits<double>::infinity();
        candidate.minimum_clearance = 0.0;
        candidate.admissible = false;
        ++candidate_index;
      }
    }
    return candidate_index;
  }

  // 후보 (v,w)를 unicycle 식으로 전개하고 모든 장애물과의 최소 거리를 계산한다.
  void evaluate_candidate(Candidate & candidate, const PlannerConfig & config) const
  {
    double x = 0.0;
    double y = 0.0;
    double yaw = 0.0;
    double minimum_clearance = std::numeric_limits<double>::infinity();
    const std::size_t simulation_steps = static_cast<std::size_t>(std::ceil(
        config.simulation_horizon / kSimulationStepSeconds));

    for (std::size_t step = 0U; step < simulation_steps; ++step) {
      // unicycle motion model: x+=v*cos(yaw)*dt, y+=v*sin(yaw)*dt, yaw+=w*dt.
      x += candidate.velocity * std::cos(yaw) * kSimulationStepSeconds;
      y += candidate.velocity * std::sin(yaw) * kSimulationStepSeconds;
      yaw = normalize_angle(
        yaw + candidate.angular_velocity * kSimulationStepSeconds);

      for (std::size_t obstacle_index = 0U; obstacle_index < obstacle_count_; ++obstacle_index) {
        const double delta_x = obstacles_[obstacle_index].x - x;
        const double delta_y = obstacles_[obstacle_index].y - y;
        const double clearance = std::hypot(delta_x, delta_y);
        minimum_clearance = std::min(minimum_clearance, clearance);
      }

      if (minimum_clearance <= config.robot_radius) {
        candidate.minimum_clearance = minimum_clearance;
        return;
      }
    }

    // 정지거리 v^2/(2a)에 로봇 반지름을 더한 값보다 장애물이 가까우면 후보를 버린다.
    const double braking_distance =
      candidate.velocity * candidate.velocity / (2.0 * config.max_acceleration);
    if (minimum_clearance <= config.robot_radius + braking_distance) {
      candidate.minimum_clearance = minimum_clearance;
      return;
    }

    const double goal_direction = std::atan2(config.goal_y - y, config.goal_x - x);
    const double heading_error = normalize_angle(goal_direction - yaw);
    // cos(error)를 [0,1]로 옮겨 목표를 바라볼수록 1점에 가깝게 한다.
    const double heading_score = 0.5 * (std::cos(heading_error) + 1.0);
    const double speed_score = candidate.velocity / config.max_velocity;
    // 여유도는 2 m 이상에서 포화시켜 먼 장애물 하나가 점수를 독점하지 못하게 한다.
    const double clearance_score = std::clamp(
      (minimum_clearance - config.robot_radius) / 2.0, 0.0, 1.0);

    candidate.score =
      config.heading_weight * heading_score +
      config.clearance_weight * clearance_score +
      config.speed_weight * speed_score;
    candidate.minimum_clearance = minimum_clearance;
    candidate.admissible = true;
  }

  // 최신 센서 snapshot 하나로 모든 후보를 평가하고 최고 점수 명령을 게시한다.
  void plan_once()
  {
    if (!have_scan_ || !have_odometry_) {
      if (!waiting_logged_) {
        RCLCPP_WARN(get_logger(), "waiting for both /scan and /odom");
        waiting_logged_ = true;
      }
      return;
    }

    const PlannerConfig config = load_config();
    const std::size_t candidate_count = build_dynamic_window(config);
    Candidate * best_candidate = nullptr;
    std::size_t admissible_count = 0U;

    for (std::size_t index = 0U; index < candidate_count; ++index) {
      evaluate_candidate(candidates_[index], config);
      if (!candidates_[index].admissible) {
        continue;
      }
      ++admissible_count;
      if (best_candidate == nullptr || candidates_[index].score > best_candidate->score) {
        best_candidate = &candidates_[index];
      }
    }

    auto command = std::make_unique<geometry_msgs::msg::TwistStamped>();
    command->header.stamp = now();
    command->header.frame_id = "base_link";
    if (best_candidate != nullptr) {
      command->twist.linear.x = best_candidate->velocity;
      command->twist.angular.z = best_candidate->angular_velocity;
    }

    const void * const command_address = command.get();
    raw_command_publisher_->publish(std::move(command));

    ++plan_count_;
    if (plan_count_ % 10U == 1U) {
      RCLCPP_INFO(
        get_logger(),
        "DWA candidates=%zu admissible=%zu best(v=%.3f,w=%.3f,clear=%.3f) unique_ptr=%p",
        candidate_count, admissible_count,
        best_candidate != nullptr ? best_candidate->velocity : 0.0,
        best_candidate != nullptr ? best_candidate->angular_velocity : 0.0,
        best_candidate != nullptr ? best_candidate->minimum_clearance : 0.0,
        command_address);
    }
  }

  bool is_managed_parameter(const std::string & name) const
  {
    return name == "goal_x" || name == "goal_y" || name == "max_velocity" ||
           name == "max_angular_velocity" || name == "max_acceleration" ||
           name == "max_angular_acceleration" || name == "robot_radius" ||
           name == "simulation_horizon" || name == "heading_weight" ||
           name == "clearance_weight" || name == "speed_weight";
  }

  rcl_interfaces::msg::SetParametersResult validate_parameters(
    const std::vector<rclcpp::Parameter> & parameters) const
  {
    auto result = rcl_interfaces::msg::SetParametersResult();
    result.successful = true;
    double heading = heading_weight_.load(std::memory_order_relaxed);
    double clearance = clearance_weight_.load(std::memory_order_relaxed);
    double speed = speed_weight_.load(std::memory_order_relaxed);

    for (const auto & parameter : parameters) {
      const std::string & name = parameter.get_name();
      if (!is_managed_parameter(name)) {
        continue;
      }
      if (parameter.get_type() != rclcpp::ParameterType::PARAMETER_DOUBLE) {
        result.successful = false;
        result.reason = name + " must be a double";
        return result;
      }

      const double value = parameter.as_double();
      if (!std::isfinite(value)) {
        result.successful = false;
        result.reason = name + " must be finite";
        return result;
      }
      if ((name == "max_velocity" && (value < 0.05 || value > 1.50)) ||
        (name == "max_angular_velocity" && (value < 0.10 || value > 3.00)) ||
        (name == "max_acceleration" && (value < 0.05 || value > 3.00)) ||
        (name == "max_angular_acceleration" && (value < 0.10 || value > 6.00)) ||
        (name == "robot_radius" && (value < 0.10 || value > 1.00)) ||
        (name == "simulation_horizon" && (value < 0.30 || value > 3.00)) ||
        ((name == "heading_weight" || name == "clearance_weight" ||
        name == "speed_weight") && (value < 0.0 || value > 2.0)))
      {
        result.successful = false;
        result.reason = name + " is outside its safe study range";
        return result;
      }

      if (name == "heading_weight") {
        heading = value;
      } else if (name == "clearance_weight") {
        clearance = value;
      } else if (name == "speed_weight") {
        speed = value;
      }
    }

    if (heading + clearance + speed <= 0.0) {
      result.successful = false;
      result.reason = "at least one DWA score weight must be positive";
    }
    return result;
  }

  // 검증 완료 뒤 바뀐 값만 원자 변수에 반영한다.
  void commit_parameters(const std::vector<rclcpp::Parameter> & parameters)
  {
    for (const auto & parameter : parameters) {
      const std::string & name = parameter.get_name();
      if (!is_managed_parameter(name)) {
        continue;
      }
      const double value = parameter.as_double();
      if (name == "goal_x") {
        goal_x_.store(value, std::memory_order_relaxed);
      } else if (name == "goal_y") {
        goal_y_.store(value, std::memory_order_relaxed);
      } else if (name == "max_velocity") {
        max_velocity_.store(value, std::memory_order_relaxed);
      } else if (name == "max_angular_velocity") {
        max_angular_velocity_.store(value, std::memory_order_relaxed);
      } else if (name == "max_acceleration") {
        max_acceleration_.store(value, std::memory_order_relaxed);
      } else if (name == "max_angular_acceleration") {
        max_angular_acceleration_.store(value, std::memory_order_relaxed);
      } else if (name == "robot_radius") {
        robot_radius_.store(value, std::memory_order_relaxed);
      } else if (name == "simulation_horizon") {
        simulation_horizon_.store(value, std::memory_order_relaxed);
      } else if (name == "heading_weight") {
        heading_weight_.store(value, std::memory_order_relaxed);
      } else if (name == "clearance_weight") {
        clearance_weight_.store(value, std::memory_order_relaxed);
      } else if (name == "speed_weight") {
        speed_weight_.store(value, std::memory_order_relaxed);
      }
    }
    RCLCPP_INFO(get_logger(), "validated DWA parameters committed");
  }

  std::array<ObstaclePoint, kMaxObstacles> obstacles_{};
  std::array<Candidate, kMaxCandidates> candidates_{};
  std::size_t obstacle_count_{0U};
  double current_velocity_{0.0};
  double current_angular_velocity_{0.0};
  bool have_scan_{false};
  bool have_odometry_{false};
  bool waiting_logged_{false};
  std::uint64_t scan_count_{0U};
  std::uint64_t plan_count_{0U};

  std::atomic<double> goal_x_{3.0};
  std::atomic<double> goal_y_{0.8};
  std::atomic<double> max_velocity_{0.80};
  std::atomic<double> max_angular_velocity_{1.20};
  std::atomic<double> max_acceleration_{0.80};
  std::atomic<double> max_angular_acceleration_{1.80};
  std::atomic<double> robot_radius_{0.25};
  std::atomic<double> simulation_horizon_{1.50};
  std::atomic<double> heading_weight_{0.45};
  std::atomic<double> clearance_weight_{0.35};
  std::atomic<double> speed_weight_{0.20};

  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr
    parameter_validation_handle_;
  rclcpp::node_interfaces::PostSetParametersCallbackHandle::SharedPtr
    parameter_commit_handle_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_subscription_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometry_subscription_;
  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr raw_command_publisher_;
  rclcpp::TimerBase::SharedPtr planning_timer_;
};

}  // namespace daily_robotics_2026_09_07

RCLCPP_COMPONENTS_REGISTER_NODE(
  daily_robotics_2026_09_07::DwaPlannerComponent)
