// rclcpp.hpp는 ROS 2 C++ 노드, publisher, subscriber, parameter API를 제공합니다.
#include <rclcpp/rclcpp.hpp>

// geometry_msgs/msg/twist.hpp는 로봇 속도 명령 메시지인 geometry_msgs::msg::Twist를 제공합니다.
#include <geometry_msgs/msg/twist.hpp>

// sensor_msgs/msg/laser_scan.hpp는 2D LiDAR 데이터 메시지인 sensor_msgs::msg::LaserScan을 제공합니다.
#include <sensor_msgs/msg/laser_scan.hpp>

// algorithm은 std::clamp, std::min_element 같은 표준 알고리즘을 제공합니다.
#include <algorithm>

// cmath는 std::isfinite 같은 수학 함수를 제공합니다.
#include <cmath>

// memory는 std::shared_ptr 같은 스마트 포인터를 제공합니다.
#include <memory>

// string은 std::string 타입을 제공합니다.
#include <string>

// vector는 동적 배열인 std::vector를 제공합니다.
#include <vector>

// PolicyOutput은 모델이 계산한 선속도와 각속도를 묶는 작은 구조체입니다.
struct PolicyOutput
{
  // linear_x는 로봇 전방 속도입니다. 단위는 보통 meter/second입니다.
  double linear_x{0.0};

  // angular_z는 로봇이 z축 기준으로 회전하는 속도입니다. 단위는 보통 radian/second입니다.
  double angular_z{0.0};
};

// MlPolicyNode는 rclcpp::Node를 상속받는 ROS 2 C++ 노드 클래스입니다.
class MlPolicyNode : public rclcpp::Node
{
public:
  // 생성자는 노드 객체가 만들어질 때 한 번 실행됩니다.
  MlPolicyNode()
  // Node 생성자에 넘긴 문자열은 ROS 그래프에 보이는 노드 이름입니다.
  : Node("ml_policy_node")
  {
    // declare_parameter는 parameter 이름과 기본값을 등록합니다.
    this->declare_parameter<std::string>("scan_topic", "/scan");

    // raw_command_topic은 safety filter로 넘길 원시 정책 명령 topic입니다.
    this->declare_parameter<std::string>("raw_command_topic", "/policy/cmd_vel_raw");

    // model_path는 ONNX 모델 파일 경로입니다. 이 예제에서는 fallback 정책을 쓰지만 실무에서는 사용합니다.
    this->declare_parameter<std::string>("model_path", "policy.onnx");

    // max_forward_speed는 정책 출력의 최대 전진 속도 제한입니다.
    this->declare_parameter<double>("max_forward_speed", 0.25);

    // max_turn_speed는 정책 출력의 최대 회전 속도 제한입니다.
    this->declare_parameter<double>("max_turn_speed", 0.6);

    // obstacle_stop_distance는 장애물이 너무 가까울 때 정지할 거리입니다.
    this->declare_parameter<double>("obstacle_stop_distance", 0.45);

    // dry_run이 true이면 계산은 하지만 실제 명령 publish는 하지 않습니다.
    this->declare_parameter<bool>("dry_run", false);

    // get_parameter(...).as_string()은 string parameter 값을 읽습니다.
    scan_topic_ = this->get_parameter("scan_topic").as_string();

    // 원시 명령 topic 이름을 parameter에서 읽습니다.
    raw_command_topic_ = this->get_parameter("raw_command_topic").as_string();

    // 모델 경로를 parameter에서 읽습니다.
    model_path_ = this->get_parameter("model_path").as_string();

    // 속도 제한 parameter를 읽습니다.
    max_forward_speed_ = this->get_parameter("max_forward_speed").as_double();

    // 회전 속도 제한 parameter를 읽습니다.
    max_turn_speed_ = this->get_parameter("max_turn_speed").as_double();

    // 장애물 정지 거리 parameter를 읽습니다.
    obstacle_stop_distance_ = this->get_parameter("obstacle_stop_distance").as_double();

    // dry_run parameter를 읽습니다.
    dry_run_ = this->get_parameter("dry_run").as_bool();

    // create_publisher는 Twist 메시지를 raw_command_topic_으로 publish하는 publisher를 만듭니다.
    raw_cmd_pub_ = this->create_publisher<geometry_msgs::msg::Twist>(raw_command_topic_, 10);

    // create_subscription은 LaserScan topic을 구독하고 메시지가 오면 on_scan 함수를 호출합니다.
    scan_sub_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
      scan_topic_,
      rclcpp::SensorDataQoS(),
      std::bind(&MlPolicyNode::on_scan, this, std::placeholders::_1));

    // 시작 로그에는 어떤 topic과 모델 경로를 사용하는지 남깁니다.
    RCLCPP_INFO(
      this->get_logger(),
      "ml_policy_node started. scan_topic=%s raw_command_topic=%s model_path=%s dry_run=%s",
      scan_topic_.c_str(),
      raw_command_topic_.c_str(),
      model_path_.c_str(),
      dry_run_ ? "true" : "false");
  }

private:
  // on_scan은 /scan 메시지가 들어올 때마다 호출되는 subscriber callback입니다.
  void on_scan(const sensor_msgs::msg::LaserScan::SharedPtr scan)
  {
    // preprocess_scan은 LaserScan ranges를 모델 입력 vector로 바꿉니다.
    const std::vector<float> input = preprocess_scan(*scan);

    // run_policy는 모델 추론을 수행하고 전진/회전 명령을 계산합니다.
    PolicyOutput output = run_policy(input);

    // clamp_policy_output은 정책 출력이 설정한 속도 제한을 넘지 않게 자릅니다.
    output = clamp_policy_output(output);

    // make_twist는 PolicyOutput 구조체를 ROS 2 Twist 메시지로 변환합니다.
    geometry_msgs::msg::Twist command = make_twist(output);

    // dry_run이면 publish하지 않고 로그만 남깁니다.
    if (dry_run_) {
      RCLCPP_INFO(
        this->get_logger(),
        "dry-run command linear.x=%.3f angular.z=%.3f",
        command.linear.x,
        command.angular.z);
      return;
    }

    // publish는 ROS 2 topic으로 메시지를 내보냅니다.
    raw_cmd_pub_->publish(command);
  }

  // preprocess_scan은 LaserScan을 고정 길이 float vector로 변환합니다.
  std::vector<float> preprocess_scan(const sensor_msgs::msg::LaserScan & scan) const
  {
    // 입력 vector 크기는 scan.ranges와 같게 시작합니다.
    std::vector<float> input;

    // reserve는 vector capacity를 미리 잡아 push_back 비용을 줄입니다.
    input.reserve(scan.ranges.size());

    // range 값 하나하나를 순회합니다.
    for (const float range : scan.ranges) {
      // std::isfinite는 값이 NaN이나 infinity가 아닌지 검사합니다.
      const bool valid = std::isfinite(range);

      // 유효하지 않은 값은 scan.range_max로 대체합니다.
      const float safe_range = valid ? range : scan.range_max;

      // 0으로 나누지 않도록 range_max가 양수인지 확인합니다.
      const float denominator = scan.range_max > 0.0F ? scan.range_max : 1.0F;

      // 거리를 0~1 근처 값으로 정규화합니다.
      const float normalized = std::clamp(safe_range / denominator, 0.0F, 1.0F);

      // push_back은 vector 끝에 값을 추가합니다.
      input.push_back(normalized);
    }

    // 전처리된 vector를 반환합니다.
    return input;
  }

  // run_policy는 ML 모델 추론 위치입니다.
  PolicyOutput run_policy(const std::vector<float> & input) const
  {
    // 실제 프로젝트에서는 여기에서 ONNX Runtime session.Run(...)을 호출합니다.
    // 이 예제는 ROS 2 연결 구조 학습용이므로 간단한 fallback 정책을 사용합니다.

    // input이 비어 있으면 안전하게 정지합니다.
    if (input.empty()) {
      return PolicyOutput{0.0, 0.0};
    }

    // 가장 가까운 장애물 거리를 0~1 정규화 값 기준으로 찾습니다.
    const float min_normalized_range = *std::min_element(input.begin(), input.end());

    // 가까운 장애물이 있으면 전진하지 않고 회전합니다.
    if (min_normalized_range < 0.15F) {
      return PolicyOutput{0.0, max_turn_speed_ * 0.5};
    }

    // 왼쪽 절반과 오른쪽 절반의 평균 거리를 비교하기 위한 누적 변수입니다.
    double left_sum = 0.0;

    // 오른쪽 절반 평균을 위한 누적 변수입니다.
    double right_sum = 0.0;

    // 절반 index를 계산합니다.
    const std::size_t half = input.size() / 2;

    // 왼쪽 절반 값을 더합니다.
    for (std::size_t index = 0; index < half; ++index) {
      left_sum += input[index];
    }

    // 오른쪽 절반 값을 더합니다.
    for (std::size_t index = half; index < input.size(); ++index) {
      right_sum += input[index];
    }

    // 평균을 계산할 때 0으로 나누지 않도록 max를 사용합니다.
    const double left_mean = left_sum / static_cast<double>(std::max<std::size_t>(half, 1));

    // 오른쪽 요소 수를 계산합니다.
    const std::size_t right_count = input.size() - half;

    // 오른쪽 평균을 계산합니다.
    const double right_mean = right_sum / static_cast<double>(std::max<std::size_t>(right_count, 1));

    // 오른쪽이 더 넓으면 오른쪽으로 조금 회전하고, 왼쪽이 더 넓으면 왼쪽으로 회전합니다.
    const double turn = std::clamp(right_mean - left_mean, -1.0, 1.0) * max_turn_speed_;

    // 충분히 안전하면 천천히 전진합니다.
    const double forward = max_forward_speed_ * 0.8;

    // 계산한 정책 출력을 반환합니다.
    return PolicyOutput{forward, turn};
  }

  // clamp_policy_output은 정책 출력이 parameter 제한을 넘지 않도록 보호합니다.
  PolicyOutput clamp_policy_output(const PolicyOutput & output) const
  {
    // std::clamp(value, low, high)는 값이 범위를 벗어나지 않게 자릅니다.
    PolicyOutput clamped;

    // 전진 속도는 0 이상 max_forward_speed_ 이하로 제한합니다.
    clamped.linear_x = std::clamp(output.linear_x, 0.0, max_forward_speed_);

    // 회전 속도는 -max_turn_speed_ 이상 +max_turn_speed_ 이하로 제한합니다.
    clamped.angular_z = std::clamp(output.angular_z, -max_turn_speed_, max_turn_speed_);

    // 제한된 결과를 반환합니다.
    return clamped;
  }

  // make_twist는 내부 구조체를 ROS 2 Twist 메시지로 바꿉니다.
  geometry_msgs::msg::Twist make_twist(const PolicyOutput & output) const
  {
    // Twist 메시지 객체를 기본값 0으로 생성합니다.
    geometry_msgs::msg::Twist command;

    // linear.x는 전방 속도입니다.
    command.linear.x = output.linear_x;

    // angular.z는 평면 회전 속도입니다.
    command.angular.z = output.angular_z;

    // 나머지 축은 기본값 0을 유지합니다.
    return command;
  }

  // scan_topic_은 구독할 LaserScan topic 이름입니다.
  std::string scan_topic_;

  // raw_command_topic_은 원시 정책 명령을 publish할 topic 이름입니다.
  std::string raw_command_topic_;

  // model_path_는 실제 ONNX 모델 경로를 저장합니다.
  std::string model_path_;

  // max_forward_speed_는 전진 속도 제한입니다.
  double max_forward_speed_{0.25};

  // max_turn_speed_는 회전 속도 제한입니다.
  double max_turn_speed_{0.6};

  // obstacle_stop_distance_는 실제 ONNX 정책을 붙일 때 사용할 안전 거리입니다.
  double obstacle_stop_distance_{0.45};

  // dry_run_은 명령 publish 없이 로그만 남기는 모드입니다.
  bool dry_run_{false};

  // raw_cmd_pub_은 Twist 메시지를 publish하는 publisher입니다.
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr raw_cmd_pub_;

  // scan_sub_은 LaserScan 메시지를 받는 subscription입니다.
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
};

// main 함수는 C++ 프로그램의 시작점입니다.
int main(int argc, char ** argv)
{
  // rclcpp::init은 ROS 2 C++ 런타임을 초기화합니다.
  rclcpp::init(argc, argv);

  // std::make_shared는 MlPolicyNode 객체를 shared_ptr로 생성합니다.
  const auto node = std::make_shared<MlPolicyNode>();

  // rclcpp::spin은 노드 callback을 계속 처리합니다.
  rclcpp::spin(node);

  // rclcpp::shutdown은 ROS 2 런타임을 정리합니다.
  rclcpp::shutdown();

  // main에서 0을 반환하면 정상 종료를 의미합니다.
  return 0;
}
