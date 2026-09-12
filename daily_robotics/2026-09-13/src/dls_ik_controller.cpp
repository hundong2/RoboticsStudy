#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "controller_interface/controller_interface.hpp"
#include "geometry_msgs/msg/point.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "realtime_tools/realtime_buffer.hpp"
#include "realtime_tools/realtime_publisher.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"

namespace daily_robotics_2026_09_13
{

// 비실시간 subscription callback과 실시간 update() 사이를 고정 크기 값으로 연결한다.
struct Target2D
{
  double x{1.20};
  double y{0.20};
  std::uint64_t sequence{0U};
};

// 2R 평면 팔의 Cartesian 목표를 position command로 바꾸는 Damped Least Squares IK controller다.
class DlsIkController final : public controller_interface::ControllerInterface
{
public:
  controller_interface::CallbackReturn on_init() override
  {
    // auto_declare는 spawner가 넘긴 YAML override가 있으면 사용하고, 없으면 안전한 기본값을 선언한다.
    auto_declare<std::vector<std::string>>("joints", {"shoulder_joint", "elbow_joint"});
    auto_declare<double>("link_1_m", 1.0);
    auto_declare<double>("link_2_m", 0.8);
    auto_declare<double>("cartesian_gain", 3.0);
    auto_declare<double>("max_joint_speed_rad_s", 1.5);
    return controller_interface::CallbackReturn::SUCCESS;
  }

  controller_interface::InterfaceConfiguration command_interface_configuration() const override
  {
    controller_interface::InterfaceConfiguration config;
    config.type = controller_interface::interface_configuration_type::INDIVIDUAL;
    for (const auto & joint : joint_names_) {
      config.names.push_back(joint + "/" + hardware_interface::HW_IF_POSITION);
    }
    return config;
  }

  controller_interface::InterfaceConfiguration state_interface_configuration() const override
  {
    controller_interface::InterfaceConfiguration config;
    config.type = controller_interface::interface_configuration_type::INDIVIDUAL;
    for (const auto & joint : joint_names_) {
      config.names.push_back(joint + "/" + hardware_interface::HW_IF_POSITION);
    }
    return config;
  }

  controller_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & /*previous_state*/) override
  {
    joint_names_ = get_node()->get_parameter("joints").as_string_array();
    link_1_m_ = get_node()->get_parameter("link_1_m").as_double();
    link_2_m_ = get_node()->get_parameter("link_2_m").as_double();
    gain_ = get_node()->get_parameter("cartesian_gain").as_double();
    max_joint_speed_ = get_node()->get_parameter("max_joint_speed_rad_s").as_double();
    if (joint_names_.size() != 2U || link_1_m_ <= 0.0 || link_2_m_ <= 0.0 ||
      gain_ <= 0.0 || max_joint_speed_ <= 0.0)
    {
      RCLCPP_ERROR(get_node()->get_logger(), "2개 관절과 양수 gain/link/speed가 필요합니다");
      return controller_interface::CallbackReturn::ERROR;
    }

    // 일반 subscription callback은 DDS/executor 문맥에서 실행되므로 RT update와 직접 변수를 공유하지 않는다.
    target_subscription_ = get_node()->create_subscription<geometry_msgs::msg::Point>(
      "/arm/target_xy", rclcpp::QoS(1).reliable(),
      [this](const geometry_msgs::msg::Point::SharedPtr message) {
        const double radius = std::hypot(message->x, message->y);
        // 2R 작업공간 |l1-l2| < r < l1+l2 안의 여유 있는 목표만 RT 버퍼에 넘긴다.
        if (std::isfinite(radius) && radius > std::abs(link_1_m_ - link_2_m_) + 0.001 &&
          radius < link_1_m_ + link_2_m_ - 0.001)
        {
          Target2D next{message->x, message->y, target_sequence_.fetch_add(1U) + 1U};
          target_buffer_.writeFromNonRT(next);
        }
      });

    // rclcpp publisher 생성과 RealtimePublisher 전용 thread 시작은 비실시간 configure에서 수행한다.
    auto publisher = get_node()->create_publisher<std_msgs::msg::Float64MultiArray>(
      "~/status", rclcpp::QoS(10).reliable());
    rt_status_publisher_ =
      std::make_unique<realtime_tools::RealtimePublisher<std_msgs::msg::Float64MultiArray>>(
      publisher);

    // update() 안에서 vector resize가 일어나지 않도록 메시지 저장 공간을 미리 할당한다.
    rt_status_publisher_->msg_.data.resize(8U, 0.0);
    target_buffer_.initRT(Target2D{});
    update_count_ = 0U;
    publish_misses_ = 0U;
    interface_misses_ = 0U;
    return controller_interface::CallbackReturn::SUCCESS;
  }

  controller_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & /*previous_state*/) override
  {
    if (command_interfaces_.size() != 2U || state_interfaces_.size() != 2U) {
      return controller_interface::CallbackReturn::ERROR;
    }
    // 활성화 직후 현재 상태를 명령으로 복사해 controller switch 때 위치 점프를 막는다.
    for (std::size_t i = 0; i < 2U; ++i) {
      const auto measured = state_interfaces_[i].get_optional<double>();
      if (!measured.has_value() || !command_interfaces_[i].set_value(*measured)) {
        return controller_interface::CallbackReturn::ERROR;
      }
    }
    return controller_interface::CallbackReturn::SUCCESS;
  }

  controller_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & /*previous_state*/) override
  {
    // Interface 반환은 Controller Manager가 담당한다. 여기서는 별도 동적 자원 변경을 하지 않는다.
    return controller_interface::CallbackReturn::SUCCESS;
  }

  controller_interface::return_type update(
    const rclcpp::Time & /*time*/, const rclcpp::Duration & period) override
  {
    const auto q1_value = state_interfaces_[0].get_optional<double>();
    const auto q2_value = state_interfaces_[1].get_optional<double>();
    if (!q1_value.has_value() || !q2_value.has_value()) {
      ++interface_misses_;
      return controller_interface::return_type::OK;
    }
    const double q1 = *q1_value;
    const double q2 = *q2_value;
    const Target2D target = *target_buffer_.readFromRT();

    // 순기구학: p(q)=[l1 cos(q1)+l2 cos(q1+q2), l1 sin(q1)+l2 sin(q1+q2)].
    const double q12 = q1 + q2;
    const double x = link_1_m_ * std::cos(q1) + link_2_m_ * std::cos(q12);
    const double y = link_1_m_ * std::sin(q1) + link_2_m_ * std::sin(q12);
    const double error_x = target.x - x;
    const double error_y = target.y - y;
    const double error_norm = std::hypot(error_x, error_y);

    // J=d(x,y)/d(q1,q2): 관절 속도를 말단 Cartesian 속도로 매핑하는 2x2 Jacobian이다.
    const double j00 = -link_1_m_ * std::sin(q1) - link_2_m_ * std::sin(q12);
    const double j01 = -link_2_m_ * std::sin(q12);
    const double j10 = link_1_m_ * std::cos(q1) + link_2_m_ * std::cos(q12);
    const double j11 = link_2_m_ * std::cos(q12);

    // manipulability |det(J)|가 0에 가까우면 팔이 펴지거나 접힌 특이점이다.
    // 이때 lambda를 키워 qdot=J^T(JJ^T+lambda^2 I)^-1 v의 폭주를 억제한다.
    const double manipulability = std::abs(link_1_m_ * link_2_m_ * std::sin(q2));
    const double lambda = manipulability < 0.12 ?
      0.005 + 0.15 * (1.0 - manipulability / 0.12) : 0.005;
    const double lambda_squared = lambda * lambda;

    // 목표가 멀어도 Cartesian 속도 명령이 과도해지지 않도록 각 축을 제한한다.
    const double vx = std::clamp(gain_ * error_x, -0.8, 0.8);
    const double vy = std::clamp(gain_ * error_y, -0.8, 0.8);

    // A=JJ^T+lambda^2 I를 2x2 닫힌식으로 역행렬 계산한다. lambda>0이므로 det(A)>0이다.
    const double a00 = j00 * j00 + j01 * j01 + lambda_squared;
    const double a01 = j00 * j10 + j01 * j11;
    const double a11 = j10 * j10 + j11 * j11 + lambda_squared;
    const double inverse_determinant = 1.0 / (a00 * a11 - a01 * a01);
    const double u0 = (a11 * vx - a01 * vy) * inverse_determinant;
    const double u1 = (-a01 * vx + a00 * vy) * inverse_determinant;
    const double q1_dot = std::clamp(j00 * u0 + j10 * u1, -max_joint_speed_, max_joint_speed_);
    const double q2_dot = std::clamp(j01 * u0 + j11 * u1, -max_joint_speed_, max_joint_speed_);

    const double dt = std::clamp(period.seconds(), 0.0, 0.02);
    const double q1_command = std::clamp(q1 + q1_dot * dt, -2.8, 2.8);
    const double q2_command = std::clamp(q2 + q2_dot * dt, -2.6, 2.6);
    if (!command_interfaces_[0].set_value(q1_command) ||
      !command_interfaces_[1].set_value(q2_command))
    {
      ++interface_misses_;
    }

    // 250 Hz 제어 중 25 Hz만 진단한다. trylock 실패 시 기다리지 않고 진단 한 건을 포기한다.
    ++update_count_;
    if (update_count_ % 10U == 0U) {
      if (rt_status_publisher_->trylock()) {
        auto & data = rt_status_publisher_->msg_.data;
        data[0] = x;
        data[1] = y;
        data[2] = error_norm;
        data[3] = lambda;
        data[4] = q1;
        data[5] = q2;
        data[6] = static_cast<double>(publish_misses_);
        data[7] = static_cast<double>(interface_misses_);
        rt_status_publisher_->unlockAndPublish();
      } else {
        ++publish_misses_;
      }
    }
    return controller_interface::return_type::OK;
  }

private:
  std::vector<std::string> joint_names_;
  double link_1_m_{1.0};
  double link_2_m_{0.8};
  double gain_{3.0};
  double max_joint_speed_{1.5};

  realtime_tools::RealtimeBuffer<Target2D> target_buffer_;
  rclcpp::Subscription<geometry_msgs::msg::Point>::SharedPtr target_subscription_;
  std::unique_ptr<realtime_tools::RealtimePublisher<std_msgs::msg::Float64MultiArray>>
    rt_status_publisher_;
  std::atomic<std::uint64_t> target_sequence_{0U};
  std::uint64_t update_count_{0U};
  std::uint64_t publish_misses_{0U};
  std::uint64_t interface_misses_{0U};
};

}  // namespace daily_robotics_2026_09_13

PLUGINLIB_EXPORT_CLASS(
  daily_robotics_2026_09_13::DlsIkController,
  controller_interface::ControllerInterface)
