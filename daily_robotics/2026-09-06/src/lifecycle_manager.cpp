#include <chrono>
#include <memory>
#include <string>

#include "daily_robotics_2026_09_06/srv/reset_pose.hpp"
#include "lifecycle_msgs/msg/transition.hpp"
#include "lifecycle_msgs/srv/change_state.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

// 외부 supervisor 역할의 노드다. 관리 대상 EKF가 발견되면 configure→activate를 Service로
// 순서대로 요청하고, 활성화 뒤 사용자 정의 ResetPose Service 호출까지 비동기로 시연한다.
class LifecycleManager final : public rclcpp::Node
{
public:
  LifecycleManager()
  : Node("ekf_lifecycle_manager")
  {
    // create_client<T>는 T::Request를 보내고 T::Response를 Future로 받는 타입 안전 RPC 클라이언트다.
    change_state_client_ = create_client<lifecycle_msgs::srv::ChangeState>(
      "/planar_ekf/change_state");
    reset_pose_client_ = create_client<daily_robotics_2026_09_06::srv::ResetPose>(
      "/ekf/reset_pose");

    // 비동기 future를 쓰므로 Executor 스레드를 wait_for로 막지 않고 응답 콜백을 받을 수 있다.
    timer_ = create_wall_timer(200ms, std::bind(&LifecycleManager::advance_state_machine, this));
  }

private:
  enum class Stage
  {
    WAIT_CHANGE_SERVICE,
    CONFIGURING,
    READY_TO_ACTIVATE,
    ACTIVATING,
    WAIT_RESET_SERVICE,
    RESETTING,
    DONE,
    FAILED
  };

  // supervisor의 작은 상태 머신이다. 이전 Service 응답이 성공해야 다음 전이를 보낸다.
  void advance_state_machine()
  {
    switch (stage_) {
      case Stage::WAIT_CHANGE_SERVICE:
        if (change_state_client_->service_is_ready()) {
          send_transition(
            lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE,
            "configure", Stage::CONFIGURING, Stage::READY_TO_ACTIVATE);
        }
        break;
      case Stage::READY_TO_ACTIVATE:
        send_transition(
          lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE,
          "activate", Stage::ACTIVATING, Stage::WAIT_RESET_SERVICE);
        break;
      case Stage::WAIT_RESET_SERVICE:
        if (reset_pose_client_->service_is_ready()) {
          send_reset_request();
        }
        break;
      case Stage::CONFIGURING:
      case Stage::ACTIVATING:
      case Stage::RESETTING:
      case Stage::DONE:
      case Stage::FAILED:
        break;
    }
  }

  void send_transition(
    std::uint8_t transition_id,
    const std::string & transition_name,
    Stage waiting_stage,
    Stage success_stage)
  {
    auto request = std::make_shared<lifecycle_msgs::srv::ChangeState::Request>();
    // lifecycle_msgs 상수를 쓰면 1=configure 같은 magic number를 코드에서 제거할 수 있다.
    request->transition.id = transition_id;
    stage_ = waiting_stage;
    RCLCPP_INFO(get_logger(), "requesting %s transition", transition_name.c_str());

    // async_send_request는 요청을 DDS Service로 보내고 응답 도착 시 이 람다를 Executor가 실행한다.
    change_state_client_->async_send_request(
      request,
      [this, transition_name, success_stage](
        rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedFuture future) {
        const auto response = future.get();
        if (!response->success) {
          RCLCPP_ERROR(
            get_logger(), "%s transition was rejected", transition_name.c_str());
          stage_ = Stage::FAILED;
          return;
        }
        RCLCPP_INFO(get_logger(), "%s transition succeeded", transition_name.c_str());
        stage_ = success_stage;
      });
  }

  // 활성화된 EKF에 짧은 재초기화 RPC를 보내 Service의 요청/응답 흐름을 완성한다.
  void send_reset_request()
  {
    auto request = std::make_shared<daily_robotics_2026_09_06::srv::ResetPose::Request>();
    request->x = 0.0;
    request->y = 0.0;
    request->yaw = 0.0;
    stage_ = Stage::RESETTING;

    reset_pose_client_->async_send_request(
      request,
      [this](rclcpp::Client<daily_robotics_2026_09_06::srv::ResetPose>::SharedFuture future) {
        const auto response = future.get();
        if (!response->accepted) {
          RCLCPP_ERROR(get_logger(), "ResetPose rejected: %s", response->message.c_str());
          stage_ = Stage::FAILED;
          return;
        }
        RCLCPP_INFO(get_logger(), "ResetPose accepted: %s", response->message.c_str());
        stage_ = Stage::DONE;
        // 최종 상태 뒤에는 타이머가 필요 없으므로 취소해 불필요한 wake-up을 없앤다.
        timer_->cancel();
      });
  }

  rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedPtr change_state_client_;
  rclcpp::Client<daily_robotics_2026_09_06::srv::ResetPose>::SharedPtr reset_pose_client_;
  rclcpp::TimerBase::SharedPtr timer_;
  Stage stage_{Stage::WAIT_CHANGE_SERVICE};
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  // spin은 Timer와 두 비동기 Service 응답 콜백을 한 Executor에서 처리한다.
  rclcpp::spin(std::make_shared<LifecycleManager>());
  rclcpp::shutdown();
  return 0;
}
