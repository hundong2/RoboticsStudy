// rclcpp.hpp는 ROS 2 C++ API를 제공합니다.
#include <rclcpp/rclcpp.hpp>

// geometry_msgs/msg/twist.hpp는 속도 명령 메시지를 제공합니다.
#include <geometry_msgs/msg/twist.hpp>

// sensor_msgs/msg/laser_scan.hpp는 LiDAR 스캔 메시지를 제공합니다.
#include <sensor_msgs/msg/laser_scan.hpp>

// algorithm은 std::clamp와 std::min_element를 제공합니다.
#include <algorithm>

// cmath는 std::isfinite를 제공합니다.
#include <cmath>

// limits는 infinity 같은 숫자 한계를 제공합니다.
#include <limits>

// memory는 std::shared_ptr를 제공합니다.
#include <memory>

// string은 std::string을 제공합니다.
#include <string>

// SafetyFilter는 원시 정책 명령을 최종 /cmd_vel로 보내기 전에 제한하는 노드입니다.
class SafetyFilter : public rclcpp::Node
{
public:
  // 생성자는 publisher, subscriber, parameter를 설정합니다.
  SafetyFilter()
  // ROS 그래프에 보이는 노드 이름은 safety_filter입니다.
  : Node("safety_filter")
  {
    // raw_command_topic은 ML 정책이 낸 원시 명령 topic입니다.
    this->declare_parameter<std::string>("raw_command_topic", "/policy/cmd_vel_raw");

    // safe_command_topic은 실제 controller나 simulator가 읽는 최종 명령 topic입니다.
    this->declare_parameter<std::string>("safe_command_topic", "/cmd_vel");

    // scan_topic은 장애물 거리를 확인할 LaserScan topic입니다.
    this->declare_parameter<std::string>("scan_topic", "/scan");

    // max_forward_speed는 safety filter가 허용하는 최대 전진 속도입니다.
    this->declare_parameter<double>("max_forward_speed", 0.20);

    // max_turn_speed는 safety filter가 허용하는 최대 회전 속도입니다.
    this->declare_parameter<double>("max_turn_speed", 0.5);

    // hard_stop_distance는 이 거리보다 장애물이 가까우면 무조건 정지하는 기준입니다.
    this->declare_parameter<double>("hard_stop_distance", 0.35);

    // dry_run이 true이면 최종 명령을 publish하지 않고 로그만 남깁니다.
    this->declare_parameter<bool>("dry_run", false);

    // parameter 값을 멤버 변수로 읽습니다.
    raw_command_topic_ = this->get_parameter("raw_command_topic").as_string();

    // 최종 명령 topic 이름을 읽습니다.
    safe_command_topic_ = this->get_parameter("safe_command_topic").as_string();

    // scan topic 이름을 읽습니다.
    scan_topic_ = this->get_parameter("scan_topic").as_string();

    // 전진 속도 제한을 읽습니다.
    max_forward_speed_ = this->get_parameter("max_forward_speed").as_double();

    // 회전 속도 제한을 읽습니다.
    max_turn_speed_ = this->get_parameter("max_turn_speed").as_double();

    // 강제 정지 거리 기준을 읽습니다.
    hard_stop_distance_ = this->get_parameter("hard_stop_distance").as_double();

    // dry-run 설정을 읽습니다.
    dry_run_ = this->get_parameter("dry_run").as_bool();

    // 최종 /cmd_vel publisher를 만듭니다.
    safe_cmd_pub_ = this->create_publisher<geometry_msgs::msg::Twist>(safe_command_topic_, 10);

    // LaserScan subscriber를 만듭니다.
    scan_sub_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
      scan_topic_,
      rclcpp::SensorDataQoS(),
      std::bind(&SafetyFilter::on_scan, this, std::placeholders::_1));

    // 원시 정책 명령 subscriber를 만듭니다.
    raw_cmd_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      raw_command_topic_,
      10,
      std::bind(&SafetyFilter::on_raw_command, this, std::placeholders::_1));

    // 시작 로그를 남깁니다.
    RCLCPP_INFO(
      this->get_logger(),
      "safety_filter started. raw=%s safe=%s scan=%s",
      raw_command_topic_.c_str(),
      safe_command_topic_.c_str(),
      scan_topic_.c_str());
  }

private:
  // on_scan은 최신 장애물 거리를 계속 업데이트합니다.
  void on_scan(const sensor_msgs::msg::LaserScan::SharedPtr scan)
  {
    // 시작값을 infinity로 두면 어떤 실제 거리든 더 작게 갱신됩니다.
    double closest = std::numeric_limits<double>::infinity();

    // ranges 배열을 순회합니다.
    for (const float range : scan->ranges) {
      // NaN이나 infinity는 무시합니다.
      if (!std::isfinite(range)) {
        continue;
      }

      // 가장 작은 유효 거리를 저장합니다.
      closest = std::min(closest, static_cast<double>(range));
    }

    // 최신 장애물 거리를 멤버 변수에 저장합니다.
    closest_obstacle_m_ = closest;
  }

  // on_raw_command는 ML 정책의 원시 Twist 명령이 들어올 때 실행됩니다.
  void on_raw_command(const geometry_msgs::msg::Twist::SharedPtr raw)
  {
    // 입력 메시지를 복사해 수정 가능한 command를 만듭니다.
    geometry_msgs::msg::Twist safe = *raw;

    // 선속도는 0 이상 max_forward_speed_ 이하로 제한합니다.
    safe.linear.x = std::clamp(safe.linear.x, 0.0, max_forward_speed_);

    // 회전 속도는 -max_turn_speed_ 이상 +max_turn_speed_ 이하로 제한합니다.
    safe.angular.z = std::clamp(safe.angular.z, -max_turn_speed_, max_turn_speed_);

    // 장애물이 너무 가까우면 전진 속도를 0으로 만듭니다.
    if (closest_obstacle_m_ < hard_stop_distance_) {
      safe.linear.x = 0.0;

      // 너무 가까운 경우 회전도 줄여 로봇이 벽을 긁지 않게 합니다.
      safe.angular.z = std::clamp(safe.angular.z, -max_turn_speed_ * 0.5, max_turn_speed_ * 0.5);

      // 안전 필터가 개입했다는 로그를 남깁니다.
      RCLCPP_WARN(
        this->get_logger(),
        "hard stop active. closest_obstacle=%.3f hard_stop_distance=%.3f",
        closest_obstacle_m_,
        hard_stop_distance_);
    }

    // dry-run 모드에서는 publish하지 않습니다.
    if (dry_run_) {
      RCLCPP_INFO(
        this->get_logger(),
        "dry-run safe command linear.x=%.3f angular.z=%.3f",
        safe.linear.x,
        safe.angular.z);
      return;
    }

    // 안전 검사를 통과한 명령을 최종 topic으로 publish합니다.
    safe_cmd_pub_->publish(safe);
  }

  // raw_command_topic_은 원시 정책 명령 topic 이름입니다.
  std::string raw_command_topic_;

  // safe_command_topic_은 최종 명령 topic 이름입니다.
  std::string safe_command_topic_;

  // scan_topic_은 LaserScan topic 이름입니다.
  std::string scan_topic_;

  // max_forward_speed_는 safety filter의 전진 속도 제한입니다.
  double max_forward_speed_{0.20};

  // max_turn_speed_는 safety filter의 회전 속도 제한입니다.
  double max_turn_speed_{0.5};

  // hard_stop_distance_는 강제 정지 거리입니다.
  double hard_stop_distance_{0.35};

  // dry_run_은 publish 없이 로그만 남기는 모드입니다.
  bool dry_run_{false};

  // closest_obstacle_m_는 최신 scan에서 찾은 가장 가까운 장애물 거리입니다.
  double closest_obstacle_m_{std::numeric_limits<double>::infinity()};

  // safe_cmd_pub_은 최종 Twist 명령 publisher입니다.
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr safe_cmd_pub_;

  // scan_sub_은 LaserScan subscriber입니다.
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;

  // raw_cmd_sub_은 원시 Twist 명령 subscriber입니다.
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr raw_cmd_sub_;
};

// main은 C++ 실행 파일의 시작점입니다.
int main(int argc, char ** argv)
{
  // ROS 2 C++ 런타임을 초기화합니다.
  rclcpp::init(argc, argv);

  // SafetyFilter 노드를 shared_ptr로 생성합니다.
  const auto node = std::make_shared<SafetyFilter>();

  // callback 처리를 위해 노드를 실행합니다.
  rclcpp::spin(node);

  // ROS 2 런타임을 종료합니다.
  rclcpp::shutdown();

  // 0은 정상 종료를 의미합니다.
  return 0;
}
