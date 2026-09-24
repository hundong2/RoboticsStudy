#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

// 같은 패키지의 .action에서 생성된 Goal/Feedback/Result 타입이다.
#include "daily_robotics_2026_09_25/action/execute_joint_trajectory.hpp"

using namespace std::chrono_literals;

/**
 * 서버 발견 뒤 3축 비영점 경계조건 Goal을 한 번 보내고 Feedback/Result 계약을 검증하는 노드다.
 * 실제 애플리케이션에서는 MoveIt이나 상위 task planner가 이 ActionClient 역할을 맡는다.
 */
class TrajectoryGoalClient : public rclcpp::Node
{
public:
  using ExecuteJointTrajectory =
    daily_robotics_2026_09_25::action::ExecuteJointTrajectory;
  using GoalHandleTrajectory =
    rclcpp_action::ClientGoalHandle<ExecuteJointTrajectory>;

  TrajectoryGoalClient()
  : Node("trajectory_goal_client")
  {
    // create_client는 Goal/Cancel/Result service와 Feedback/Status topic을 하나의 타입 안전 API로 묶는다.
    client_ = rclcpp_action::create_client<ExecuteJointTrajectory>(
      this, "/execute_joint_trajectory");

    // launch 순서와 DDS discovery 시간은 일정하지 않으므로 비차단 timer에서 준비 여부를 재확인한다.
    send_timer_ = create_wall_timer(
      100ms, std::bind(&TrajectoryGoalClient::send_once_when_ready, this));
  }

private:
  /** Action 서버가 discovery된 최초 시점에만 예제 Goal을 비동기로 전송한다. */
  void send_once_when_ready()
  {
    if (goal_sent_ || !client_->wait_for_action_server(0s)) {
      return;
    }
    goal_sent_ = true;
    send_timer_->cancel();

    ExecuteJointTrajectory::Goal goal;
    // 정지→정지가 아닌 경계조건으로 위치뿐 아니라 속도·가속도 연속성까지 시험한다.
    goal.start_position_rad = std::array<double, 3>{{-0.40, 0.25, -0.10}};
    goal.start_velocity_rad_s = std::array<double, 3>{{0.12, -0.08, 0.05}};
    goal.start_acceleration_rad_s2 = std::array<double, 3>{{0.15, -0.10, 0.08}};
    goal.goal_position_rad = std::array<double, 3>{{0.80, -0.50, 0.35}};
    goal.goal_velocity_rad_s = std::array<double, 3>{{-0.10, 0.06, -0.04}};
    goal.goal_acceleration_rad_s2 = std::array<double, 3>{{-0.12, 0.08, 0.00}};
    goal.duration.sec = 2;
    goal.duration.nanosec = 600'000'000U;
    goal.position_tolerance_rad = 0.025;

    // SendGoalOptions는 수락 응답, 진행 Feedback, 최종 Result의 callback을 각각 명시한다.
    rclcpp_action::Client<ExecuteJointTrajectory>::SendGoalOptions options;
    options.goal_response_callback =
      [this](const GoalHandleTrajectory::SharedPtr & goal_handle) {
        if (!goal_handle) {
          RCLCPP_ERROR(get_logger(), "CLIENT_FAIL reason=goal_rejected");
          rclcpp::shutdown();
          return;
        }
        RCLCPP_INFO(get_logger(), "Goal accepted: 3 axes, T=2.6 s, tolerance=0.025 rad");
      };

    options.feedback_callback =
      [this](
      GoalHandleTrajectory::SharedPtr,
      const std::shared_ptr<const ExecuteJointTrajectory::Feedback> feedback) {
        ++feedback_count_;
        max_feedback_error_ = std::max(
          max_feedback_error_, feedback->current_position_error_rad);
        // 25개마다만 로그해 관측성은 유지하면서 stdout/DDS 외부 비용을 제한한다.
        if (feedback_count_ % 25U == 0U) {
          const double elapsed = static_cast<double>(feedback->elapsed.sec) +
            static_cast<double>(feedback->elapsed.nanosec) / 1'000'000'000.0;
          RCLCPP_INFO(
            get_logger(), "feedback=%zu elapsed=%.3f s current_error=%.6f rad",
            feedback_count_, elapsed, feedback->current_position_error_rad);
        }
      };

    options.result_callback =
      [this](const GoalHandleTrajectory::WrappedResult & wrapped) {
        const bool passed =
          wrapped.code == rclcpp_action::ResultCode::SUCCEEDED && wrapped.result &&
          wrapped.result->success && feedback_count_ >= 80U &&
          wrapped.result->control_ticks >= 1000U &&
          std::isfinite(wrapped.result->max_position_error_rad) &&
          wrapped.result->max_position_error_rad <= 0.025;

        if (passed) {
          RCLCPP_INFO(
            get_logger(),
            "CLIENT_PASS feedback=%zu ticks=%lu max_error=%.6f feedback_max=%.6f",
            feedback_count_, static_cast<unsigned long>(wrapped.result->control_ticks),
            wrapped.result->max_position_error_rad, max_feedback_error_);
        } else {
          RCLCPP_ERROR(
            get_logger(), "CLIENT_FAIL code=%d feedback=%zu result=%s",
            static_cast<int>(wrapped.code), feedback_count_,
            wrapped.result ? wrapped.result->message.c_str() : "missing_result");
        }
        // 이 클라이언트 프로세스의 spin만 종료하며 서버와 auditor 프로세스에는 영향을 주지 않는다.
        rclcpp::shutdown();
      };

    // async_send_goal은 executor thread를 기다리게 하지 않고 Goal request를 전송한다.
    client_->async_send_goal(goal, options);
  }

  rclcpp_action::Client<ExecuteJointTrajectory>::SharedPtr client_;
  rclcpp::TimerBase::SharedPtr send_timer_;
  bool goal_sent_{false};
  std::size_t feedback_count_{0};
  double max_feedback_error_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // spin은 Action response/feedback/result callback을 준비되는 순서대로 실행한다.
  rclcpp::spin(std::make_shared<TrajectoryGoalClient>());
  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }
  return 0;
}
