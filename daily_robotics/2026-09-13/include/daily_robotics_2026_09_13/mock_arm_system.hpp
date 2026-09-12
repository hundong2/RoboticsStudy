#ifndef DAILY_ROBOTICS_2026_09_13__MOCK_ARM_SYSTEM_HPP_
#define DAILY_ROBOTICS_2026_09_13__MOCK_ARM_SYSTEM_HPP_

#include <array>
#include <memory>
#include <vector>

#include "hardware_interface/system_interface.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

namespace daily_robotics_2026_09_13
{

// ros2_control의 SystemInterface는 여러 관절을 하나의 통신 채널로 다루는 하드웨어 추상화다.
// 이 학습용 구현은 실제 EtherCAT 드라이버 대신 위치 명령을 속도 제한 1차 모델로 추종한다.
class MockArmSystem final : public hardware_interface::SystemInterface
{
public:
  using CallbackReturn =
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  // URDF의 <ros2_control> 정보를 검사하고, 동적 할당이 필요한 준비는 비실시간 단계에서 끝낸다.
  CallbackReturn on_init(
    const hardware_interface::HardwareComponentInterfaceParams & params) override;

  // Framework가 URDF의 interface description으로 핸들을 만든 뒤, RT 루프에서 재사용할 포인터를 캐시한다.
  std::vector<hardware_interface::StateInterface::ConstSharedPtr>
  on_export_state_interfaces() override;
  std::vector<hardware_interface::CommandInterface::SharedPtr>
  on_export_command_interfaces() override;

  // configure는 통신 준비, activate는 구동 전원 허용에 대응한다. 실제 장비라면 여기서 드라이버/브레이크를 다룬다.
  CallbackReturn on_configure(const rclcpp_lifecycle::State & previous_state) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State & previous_state) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State & previous_state) override;

  // Controller Manager의 read -> controller update -> write 주기에서 호출되는 하드웨어 hot path다.
  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;
  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  static constexpr std::size_t kJointCount = 2U;
  static constexpr double kMaxVelocityRadS = 2.0;

  std::array<hardware_interface::StateInterface::SharedPtr, kJointCount> state_handles_{};
  std::array<hardware_interface::CommandInterface::SharedPtr, kJointCount> command_handles_{};
  std::array<double, kJointCount> measured_position_{};
  std::array<double, kJointCount> accepted_command_{};
};

}  // namespace daily_robotics_2026_09_13

#endif  // DAILY_ROBOTICS_2026_09_13__MOCK_ARM_SYSTEM_HPP_
