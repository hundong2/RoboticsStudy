#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

// mlockall은 현재/미래 메모리 페이지를 RAM에 고정해 제어 중 page fault를 줄이는 Linux API다.
#include <sys/mman.h>
// pthread_setschedparam은 현재 스레드에 SCHED_FIFO 우선순위를 요청하는 POSIX API다.
#include <pthread.h>
#include <time.h>

#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

// rosidl_generate_interfaces가 DriveDistance.action에서 생성한 C++ 타입이다.
#include "daily_robotics_2026_09_05/action/drive_distance.hpp"

using namespace std::chrono_literals;

/**
 * DriveDistance Action의 수명주기와 100 Hz 속도 제어를 연결하는 노드다.
 *
 * Action Goal/Cancel/Feedback/Result 처리는 일반 ROS executor 스레드에서 수행하고,
 * 일정 주기의 속도 계산은 별도 제어 스레드에서 수행한다. 두 실행 문맥은 mutex 대신
 * 원자 변수로 작은 수치 상태만 넘겨 우선순위 역전과 비결정적 대기 가능성을 줄인다.
 */
class DriveActionServer : public rclcpp::Node
{
public:
  using DriveDistance = daily_robotics_2026_09_05::action::DriveDistance;
  using GoalHandleDrive = rclcpp_action::ServerGoalHandle<DriveDistance>;

  DriveActionServer()
  : Node("drive_action_server")
  {
    // 파라미터로 주파수와 우선순위를 노출하면 코드를 다시 빌드하지 않고 장비별 튜닝이 가능하다.
    control_frequency_hz_ = this->declare_parameter<double>("control_frequency_hz", 100.0);
    rt_priority_ = this->declare_parameter<int>("rt_priority", 60);

    // KeepLast(1)+reliable: 최신 속도 명령 하나만 필요하지만 제어 명령 유실은 피하려는 선택이다.
    cmd_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>(
      "/cmd_vel", rclcpp::QoS(1).reliable());

    // Odometry도 최신 위치 하나만 읽는다. 콜백은 x 좌표만 atomic에 복사하고 즉시 반환한다.
    odom_subscription_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "/wheel/odom", rclcpp::QoS(1).reliable(),
      [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
        odom_x_m_.store(msg->pose.pose.position.x, std::memory_order_relaxed);
      });

    // create_server는 하나의 Action 이름에 Goal·Cancel·Accepted 콜백을 등록한다.
    action_server_ = rclcpp_action::create_server<DriveDistance>(
      this,
      "/drive_distance",
      std::bind(&DriveActionServer::handle_goal, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&DriveActionServer::handle_cancel, this, std::placeholders::_1),
      std::bind(&DriveActionServer::handle_accepted, this, std::placeholders::_1));

    // Feedback와 최종 Result는 10 Hz 비 RT 타이머에서 처리한다. DDS 작업을 RT 계산에서 분리하는 경계다.
    feedback_timer_ = this->create_wall_timer(100ms, std::bind(&DriveActionServer::publish_feedback_or_result, this));

    // 스레드는 노드 생성 때 한 번만 만든다. Goal마다 생성하면 지연과 할당 비용이 Goal 시점마다 달라진다.
    control_thread_ = std::thread(&DriveActionServer::control_loop, this);
  }

  ~DriveActionServer() override
  {
    // 종료 플래그를 먼저 내리고 join해야 this가 파괴된 뒤 제어 스레드가 멤버에 접근하지 않는다.
    running_.store(false, std::memory_order_release);
    if (control_thread_.joinable()) {
      control_thread_.join();
    }
  }

private:
  /** 새 Goal의 수치 범위와 단일-Goal 정책을 검사하는 executor 콜백이다. */
  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID &,
    std::shared_ptr<const DriveDistance::Goal> goal)
  {
    // NaN/Inf는 비교와 제어 계산을 오염시키므로 경계에서 거부한다.
    const bool valid = std::isfinite(goal->distance_m) && std::isfinite(goal->max_speed_mps) &&
      std::abs(goal->distance_m) >= 0.01 && goal->max_speed_mps >= 0.01 &&
      goal->max_speed_mps <= 1.0;
    if (!valid) {
      RCLCPP_WARN(this->get_logger(), "Goal rejected: distance>=0.01 m, speed in [0.01, 1.0] m/s required");
      return rclcpp_action::GoalResponse::REJECT;
    }

    // compare_exchange는 두 Goal이 거의 동시에 도착해도 정확히 하나만 자리를 예약하게 한다.
    bool expected = false;
    if (!goal_reserved_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      RCLCPP_WARN(this->get_logger(), "Goal rejected: another drive goal is active");
      return rclcpp_action::GoalResponse::REJECT;
    }
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  /** 클라이언트의 Cancel 요청을 RT 루프가 읽을 수 있는 원자 플래그로 전달한다. */
  rclcpp_action::CancelResponse handle_cancel(const std::shared_ptr<GoalHandleDrive> goal_handle)
  {
    std::lock_guard<std::mutex> lock(goal_mutex_);
    if (!active_goal_ || active_goal_ != goal_handle) {
      return rclcpp_action::CancelResponse::REJECT;
    }
    cancel_requested_.store(true, std::memory_order_release);
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  /** 수락된 Goal의 시작 위치와 제어 제한을 기록하고 RT 루프를 활성화한다. */
  void handle_accepted(const std::shared_ptr<GoalHandleDrive> goal_handle)
  {
    const auto goal = goal_handle->get_goal();
    {
      // GoalHandle은 비 RT 타이머만 사용하지만 Cancel 콜백과 동시에 접근할 수 있어 짧게 보호한다.
      std::lock_guard<std::mutex> lock(goal_mutex_);
      active_goal_ = goal_handle;
    }

    start_x_m_.store(odom_x_m_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    target_distance_m_.store(goal->distance_m, std::memory_order_relaxed);
    max_speed_mps_.store(goal->max_speed_mps, std::memory_order_relaxed);
    distance_traveled_m_.store(0.0, std::memory_order_relaxed);
    cancel_requested_.store(false, std::memory_order_relaxed);
    goal_reached_.store(false, std::memory_order_relaxed);

    // release store 이전의 모든 Goal 설정값은 acquire load로 활성 상태를 본 RT 스레드에 보인다.
    control_active_.store(true, std::memory_order_release);
    RCLCPP_INFO(
      this->get_logger(), "Accepted drive goal: distance=%.3f m, max_speed=%.3f m/s",
      goal->distance_m, goal->max_speed_mps);
  }

  /** 10 Hz로 진행률을 게시하고 성공 또는 취소 상태를 Action Result로 마감한다. */
  void publish_feedback_or_result()
  {
    std::shared_ptr<GoalHandleDrive> goal;
    {
      std::lock_guard<std::mutex> lock(goal_mutex_);
      goal = active_goal_;
    }
    if (!goal) {
      return;
    }

    const double traveled = distance_traveled_m_.load(std::memory_order_relaxed);
    const double target = target_distance_m_.load(std::memory_order_relaxed);

    if (cancel_requested_.load(std::memory_order_acquire)) {
      auto result = std::make_shared<DriveDistance::Result>();
      result->reached = false;
      result->final_distance_m = traveled;
      result->message = "Goal canceled; zero velocity requested";
      goal->canceled(result);
      reset_goal_state();
      return;
    }

    if (goal_reached_.load(std::memory_order_acquire)) {
      auto result = std::make_shared<DriveDistance::Result>();
      result->reached = true;
      result->final_distance_m = traveled;
      result->message = "Target reached within 5 mm tolerance";
      goal->succeed(result);
      reset_goal_state();
      return;
    }

    // Feedback은 장기 작업 중인 Action 클라이언트가 진행률 UI나 timeout 정책을 만들게 해준다.
    auto feedback = std::make_shared<DriveDistance::Feedback>();
    feedback->distance_traveled_m = traveled;
    feedback->distance_remaining_m = std::abs(target - traveled);
    goal->publish_feedback(feedback);
  }

  /** Goal 종료 뒤 Action 쪽 공유 상태와 제어 플래그를 다음 Goal을 위해 초기화한다. */
  void reset_goal_state()
  {
    control_active_.store(false, std::memory_order_release);
    // Cancel 플래그는 다음 Goal의 값들이 준비될 때까지 유지해 종료 경계에서 명령이 재개되는 race를 막는다.
    goal_reached_.store(false, std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(goal_mutex_);
      active_goal_.reset();
    }
    goal_reserved_.store(false, std::memory_order_release);
  }

  /** timespec에 주기를 더해 상대 sleep 누적 오차가 없는 절대 시각 deadline을 만든다. */
  static void add_nanoseconds(timespec & time, std::int64_t nanoseconds)
  {
    time.tv_nsec += static_cast<long>(nanoseconds);
    while (time.tv_nsec >= 1'000'000'000L) {
      time.tv_nsec -= 1'000'000'000L;
      ++time.tv_sec;
    }
  }

  /** 메모리 잠금과 SCHED_FIFO를 시도한다. 실패는 명확히 경고하되 교육용 노드는 계속 실행한다. */
  void configure_realtime_thread()
  {
    // MCL_CURRENT|MCL_FUTURE는 현재 매핑과 이후 매핑을 RAM에 고정한다. 보통 memlock 권한 설정이 필요하다.
    if (::mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
      RCLCPP_WARN(
        this->get_logger(), "mlockall failed (%s): continuing without locked memory",
        std::strerror(errno));
    }

    sched_param parameters{};
    parameters.sched_priority = rt_priority_;
    // SCHED_FIFO는 더 높은 우선순위의 runnable 스레드가 즉시 선점하는 RT 스케줄링 정책이다.
    const int result = ::pthread_setschedparam(pthread_self(), SCHED_FIFO, &parameters);
    if (result != 0) {
      RCLCPP_WARN(
        this->get_logger(), "SCHED_FIFO priority %d failed (%s): using normal scheduler",
        rt_priority_, std::strerror(result));
    } else {
      RCLCPP_INFO(this->get_logger(), "RT control thread enabled: SCHED_FIFO priority %d", rt_priority_);
    }

    // 스택 페이지를 미리 건드려 첫 제어 주기 중 page fault가 발생할 가능성을 줄인다.
    volatile char stack_prefault[64 * 1024];
    for (std::size_t index = 0; index < sizeof(stack_prefault); index += 4096) {
      stack_prefault[index] = 0;
    }
  }

  /** 100 Hz 절대 주기로 odometry 오차를 속도 명령으로 바꾸는 제어 스레드다. */
  void control_loop()
  {
    configure_realtime_thread();

    timespec next_wakeup{};
    ::clock_gettime(CLOCK_MONOTONIC, &next_wakeup);
    const auto period_ns = static_cast<std::int64_t>(1'000'000'000.0 / control_frequency_hz_);

    while (running_.load(std::memory_order_acquire) && rclcpp::ok()) {
      add_nanoseconds(next_wakeup, period_ns);
      // TIMER_ABSTIME은 한 주기의 지연이 다음 주기에 누적되는 drift를 막는다.
      while (::clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wakeup, nullptr) == EINTR) {
      }

      geometry_msgs::msg::Twist command;
      if (control_active_.load(std::memory_order_acquire) &&
        !cancel_requested_.load(std::memory_order_acquire))
      {
        const double start = start_x_m_.load(std::memory_order_relaxed);
        const double current = odom_x_m_.load(std::memory_order_relaxed);
        const double target = target_distance_m_.load(std::memory_order_relaxed);
        const double traveled = current - start;
        const double remaining = std::abs(target - traveled);
        distance_traveled_m_.store(traveled, std::memory_order_relaxed);

        if (remaining <= 0.005) {
          // 허용 오차 5 mm 안에서는 0 속도를 먼저 게시하고 Result 타이머에 완료를 알린다.
          command.linear.x = 0.0;
          control_active_.store(false, std::memory_order_release);
          goal_reached_.store(true, std::memory_order_release);
        } else {
          // v = sign(target) * min(v_max, 2*|e|): 멀리서는 포화, 가까이서는 감속하는 P 제어다.
          const double speed = std::min(
            max_speed_mps_.load(std::memory_order_relaxed), 2.0 * remaining);
          command.linear.x = std::copysign(speed, target);
        }
      }

      // 학습용 경계: publish 내부 DDS 경로는 할당/잠금을 포함할 수 있어 이 예제만으로 hard RT는 아니다.
      cmd_publisher_->publish(command);
    }
  }

  double control_frequency_hz_{100.0};
  int rt_priority_{60};

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_publisher_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_subscription_;
  rclcpp_action::Server<DriveDistance>::SharedPtr action_server_;
  rclcpp::TimerBase::SharedPtr feedback_timer_;

  std::thread control_thread_;
  std::atomic<bool> running_{true};
  std::atomic<bool> goal_reserved_{false};
  std::atomic<bool> control_active_{false};
  std::atomic<bool> cancel_requested_{false};
  std::atomic<bool> goal_reached_{false};
  std::atomic<double> odom_x_m_{0.0};
  std::atomic<double> start_x_m_{0.0};
  std::atomic<double> target_distance_m_{0.0};
  std::atomic<double> max_speed_mps_{0.1};
  std::atomic<double> distance_traveled_m_{0.0};

  std::mutex goal_mutex_;
  std::shared_ptr<GoalHandleDrive> active_goal_;
};

int main(int argc, char ** argv)
{
  // rclcpp::init은 CLI remapping/parameter 인수를 해석하고 ROS 2 통신 문맥을 초기화한다.
  rclcpp::init(argc, argv);
  auto node = std::make_shared<DriveActionServer>();
  // spin은 executor 이벤트 루프에서 Odometry, Action, Feedback timer 콜백을 준비되는 대로 호출한다.
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
