#include "daily_robotics_2026_09_13/mock_arm_system.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"
#include "rclcpp/logging.hpp"

namespace daily_robotics_2026_09_13
{

MockArmSystem::CallbackReturn MockArmSystem::on_init(
  const hardware_interface::HardwareComponentInterfaceParams & params)
{
  // 부모 구현이 URDF에서 joint/interface/parameter를 파싱하고 Framework 소유 저장소를 만든다.
  if (hardware_interface::SystemInterface::on_init(params) != CallbackReturn::SUCCESS) {
    return CallbackReturn::ERROR;
  }

  if (info_.joints.size() != kJointCount) {
    RCLCPP_ERROR(get_logger(), "정확히 2개 관절이 필요하지만 %zu개를 받았습니다", info_.joints.size());
    return CallbackReturn::ERROR;
  }

  // 각 관절은 position state와 position command를 하나씩 가져야 컨트롤러 계약과 일치한다.
  for (const auto & joint : info_.joints) {
    if (joint.state_interfaces.size() != 1U || joint.command_interfaces.size() != 1U ||
      joint.state_interfaces[0].name != hardware_interface::HW_IF_POSITION ||
      joint.command_interfaces[0].name != hardware_interface::HW_IF_POSITION)
    {
      RCLCPP_ERROR(get_logger(), "%s의 position state/command 계약이 잘못되었습니다", joint.name.c_str());
      return CallbackReturn::ERROR;
    }
  }
  return CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface::ConstSharedPtr>
MockArmSystem::on_export_state_interfaces()
{
  // Jazzy의 기본 구현은 URDF의 InterfaceDescription에서 thread-safe handle을 자동 생성한다.
  auto exported = hardware_interface::SystemInterface::on_export_state_interfaces();
  for (std::size_t i = 0; i < kJointCount; ++i) {
    const std::string key = info_.joints[i].name + "/" + hardware_interface::HW_IF_POSITION;
    state_handles_[i] = get_state_interface_handle(key);
  }
  return exported;
}

std::vector<hardware_interface::CommandInterface::SharedPtr>
MockArmSystem::on_export_command_interfaces()
{
  auto exported = hardware_interface::SystemInterface::on_export_command_interfaces();
  for (std::size_t i = 0; i < kJointCount; ++i) {
    const std::string key = info_.joints[i].name + "/" + hardware_interface::HW_IF_POSITION;
    command_handles_[i] = get_command_interface_handle(key);
  }
  return exported;
}

MockArmSystem::CallbackReturn MockArmSystem::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // Lifecycle callback은 RT 루프 밖이므로 초기화 때는 기다리는 set_value(..., true)를 써도 된다.
  measured_position_ = {0.35, 0.55};
  accepted_command_ = measured_position_;
  for (std::size_t i = 0; i < kJointCount; ++i) {
    if (!state_handles_[i]->set_value(measured_position_[i], true) ||
      !command_handles_[i]->set_value(accepted_command_[i], true))
    {
      return CallbackReturn::ERROR;
    }
  }
  RCLCPP_INFO(get_logger(), "CONFIGURED: 모의 버스와 encoder 저장소 준비 완료");
  return CallbackReturn::SUCCESS;
}

MockArmSystem::CallbackReturn MockArmSystem::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // Bumpless start: 활성화 순간의 측정 위치를 명령으로 복사해 갑작스러운 점프를 막는다.
  accepted_command_ = measured_position_;
  for (std::size_t i = 0; i < kJointCount; ++i) {
    if (!command_handles_[i]->set_value(accepted_command_[i], true)) {
      return CallbackReturn::ERROR;
    }
  }
  RCLCPP_INFO(get_logger(), "ACTIVE: 모의 구동 전원 허용");
  return CallbackReturn::SUCCESS;
}

MockArmSystem::CallbackReturn MockArmSystem::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // 실제 로봇에서는 토크 차단/브레이크 체결과 안전 상태 확인을 수행할 위치다.
  accepted_command_ = measured_position_;
  RCLCPP_INFO(get_logger(), "INACTIVE: 현재 위치에서 구동 명령 동결");
  return CallbackReturn::SUCCESS;
}

hardware_interface::return_type MockArmSystem::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & period)
{
  // q[k+1] = q[k] + clamp(q_cmd-q[k], -v_max*dt, +v_max*dt)
  // 이는 무한 가속도를 피한 간단한 position-servo/encoder 모델이다.
  const double dt = std::clamp(period.seconds(), 0.0, 0.02);
  const double max_step = kMaxVelocityRadS * dt;
  for (std::size_t i = 0; i < kJointCount; ++i) {
    const double error = accepted_command_[i] - measured_position_[i];
    measured_position_[i] += std::clamp(error, -max_step, max_step);

    // false는 shared_mutex를 기다리지 않는 non-blocking 접근이다. 실패하면 이번 주기를 ERROR 처리한다.
    if (!state_handles_[i]->set_value(measured_position_[i], false)) {
      return hardware_interface::return_type::ERROR;
    }
  }
  return hardware_interface::return_type::OK;
}

hardware_interface::return_type MockArmSystem::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  // Controller가 쓴 최신 명령을 버스 송신 버퍼로 옮긴다고 생각하면 된다.
  for (std::size_t i = 0; i < kJointCount; ++i) {
    const auto command = command_handles_[i]->get_optional<double>();
    if (!command.has_value() || !std::isfinite(*command)) {
      return hardware_interface::return_type::ERROR;
    }
    accepted_command_[i] = *command;
  }
  return hardware_interface::return_type::OK;
}

}  // namespace daily_robotics_2026_09_13

// pluginlib가 문자열 class name으로 공유 라이브러리의 구현을 생성하도록 등록한다.
PLUGINLIB_EXPORT_CLASS(
  daily_robotics_2026_09_13::MockArmSystem,
  hardware_interface::SystemInterface)
