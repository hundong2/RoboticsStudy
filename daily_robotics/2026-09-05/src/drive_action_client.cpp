#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

// 같은 패키지의 DriveDistance.action에서 생성된 Goal·Result·Feedback 타입이다.
#include "daily_robotics_2026_09_05/action/drive_distance.hpp"

using namespace std::chrono_literals;

/** 서버 발견 후 0.30 m 전진 Goal을 보내고 진행률과 최종 결과를 관찰하는 ActionClient 노드다. */
class DriveActionClient : public rclcpp::Node
{
public:
  using DriveDistance = daily_robotics_2026_09_05::action::DriveDistance;
  using GoalHandleDrive = rclcpp_action::ClientGoalHandle<DriveDistance>;

  DriveActionClient()
  : Node("drive_action_client")
  {
    // create_client는 /drive_distance Action의 서비스·feedback/status 채널을 내부적으로 구성한다.
    client_ = rclcpp_action::create_client<DriveDistance>(this, "/drive_distance");

    // launch 순서와 DDS 발견 시간은 비결정적이므로 timer에서 서버 준비를 비차단 방식으로 재확인한다.
    send_timer_ = this->create_wall_timer(200ms, std::bind(&DriveActionClient::send_once_when_ready, this));
  }

private:
  /** rclcpp_action의 ResultCode enum을 로그에서 바로 이해할 수 있는 문자열로 바꾼다. */
  static const char * result_code_name(const rclcpp_action::ResultCode code)
  {
    switch (code) {
      case rclcpp_action::ResultCode::SUCCEEDED:
        return "SUCCEEDED";
      case rclcpp_action::ResultCode::ABORTED:
        return "ABORTED";
      case rclcpp_action::ResultCode::CANCELED:
        return "CANCELED";
      default:
        return "UNKNOWN";
    }
  }

  /** Action 서버가 발견되는 최초 시점에 한 번만 Goal을 비동기로 보낸다. */
  void send_once_when_ready()
  {
    if (goal_sent_ || !client_->wait_for_action_server(0s)) {
      return;
    }
    goal_sent_ = true;
    send_timer_->cancel();

    DriveDistance::Goal goal;
    goal.distance_m = 0.30;
    goal.max_speed_mps = 0.15;

    // SendGoalOptions는 수락 응답, 중간 Feedback, 최종 Result의 각 콜백을 명시한다.
    rclcpp_action::Client<DriveDistance>::SendGoalOptions options;
    options.goal_response_callback =
      [this](const GoalHandleDrive::SharedPtr & goal_handle) {
        if (goal_handle) {
          RCLCPP_INFO(this->get_logger(), "Goal accepted by server");
        } else {
          RCLCPP_ERROR(this->get_logger(), "Goal rejected by server");
        }
      };
    options.feedback_callback =
      [this](
      GoalHandleDrive::SharedPtr,
      const std::shared_ptr<const DriveDistance::Feedback> feedback) {
        RCLCPP_INFO(
          this->get_logger(), "feedback: traveled=%.3f m, remaining=%.3f m",
          feedback->distance_traveled_m, feedback->distance_remaining_m);
      };
    options.result_callback =
      [this](const GoalHandleDrive::WrappedResult & wrapped) {
        if (wrapped.result) {
          RCLCPP_INFO(
            this->get_logger(), "result=%s reached=%s final=%.3f m: %s",
            result_code_name(wrapped.code), wrapped.result->reached ? "true" : "false",
            wrapped.result->final_distance_m, wrapped.result->message.c_str());
        } else {
          RCLCPP_ERROR(this->get_logger(), "Action finished without a result payload");
        }
        // 이 클라이언트 프로세스의 spin만 종료한다. 다른 launch 프로세스에는 영향을 주지 않는다.
        rclcpp::shutdown();
      };

    // async_send_goal은 executor를 막지 않고 Goal 요청을 전송한다.
    client_->async_send_goal(goal, options);
  }

  rclcpp_action::Client<DriveDistance>::SharedPtr client_;
  rclcpp::TimerBase::SharedPtr send_timer_;
  bool goal_sent_{false};
};

int main(int argc, char ** argv)
{
  // ROS 통신 초기화 후 단일 노드를 spin해 Action 관련 콜백을 처리한다.
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<DriveActionClient>());
  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }
  return 0;
}
