#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>

#include "daily_robotics_2026_09_06/srv/reset_pose.hpp"
#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "lifecycle_msgs/msg/state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"

using namespace std::chrono_literals;

// 관리 가능한 EKF 노드다. configure에서 통신 자원을 만들고 activate 뒤에만 센서를 처리해,
// 초기화되지 않은 추정치가 제어기나 Nav2로 흘러가는 일을 막는다.
class PlanarEkfLifecycle final : public rclcpp_lifecycle::LifecycleNode
{
public:
  using CallbackReturn =
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
  using Matrix3 = std::array<double, 9>;

  PlanarEkfLifecycle()
  : rclcpp_lifecycle::LifecycleNode("planar_ekf")
  {
    RCLCPP_INFO(get_logger(), "created in UNCONFIGURED; waiting for ChangeState services");
  }

  // configure 전이는 메모리와 ROS 엔티티를 준비하지만 아직 추정 결과를 내보내지 않는다.
  CallbackReturn on_configure(const rclcpp_lifecycle::State & previous_state) override
  {
    (void)previous_state;
    reset_filter(0.0, 0.0, 0.0);
    last_wheel_stamp_ = rclcpp::Time(0, 0, RCL_ROS_TIME);

    // LifecyclePublisher는 일반 Publisher와 달리 on_activate() 전 publish를 차단한다.
    pose_publisher_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/ekf/pose", rclcpp::QoS(rclcpp::KeepLast(5)).reliable());

    // 센서 Publisher와 정확히 호환되는 QoS를 요청한다. Deadline은 샘플 간격 계약이고,
    // Liveliness lease는 Publisher가 살아 있음을 증명해야 하는 heartbeat 계약이다.
    rclcpp::QoS wheel_qos(rclcpp::KeepLast(5));
    wheel_qos.reliable()
      .deadline(100ms)
      .liveliness(RMW_QOS_POLICY_LIVELINESS_MANUAL_BY_TOPIC)
      .liveliness_lease_duration(250ms);

    rclcpp::SubscriptionOptions wheel_options;
    // SubscriptionEventCallbacks는 메시지가 없어도 DDS 상태 변화로 Executor를 깨운다.
    wheel_options.event_callbacks.deadline_callback =
      [this](rclcpp::QOSDeadlineRequestedInfo & info) {
        RCLCPP_WARN(
          get_logger(), "[subscriber] requested deadline missed: +%d (total=%d)",
          info.total_count_change, info.total_count);
      };
    wheel_options.event_callbacks.liveliness_callback =
      [this](rclcpp::QOSLivelinessChangedInfo & info) {
        RCLCPP_WARN(
          get_logger(), "[subscriber] liveliness changed: alive=%d not_alive=%d delta=%d",
          info.alive_count, info.not_alive_count, info.not_alive_count_change);
      };

    // /wheel/twist는 빠른 예측(Predict), /gps/position은 느린 보정(Correct)을 유발한다.
    wheel_subscription_ = create_subscription<geometry_msgs::msg::TwistStamped>(
      "/wheel/twist", wheel_qos,
      std::bind(&PlanarEkfLifecycle::on_wheel, this, std::placeholders::_1),
      wheel_options);
    gps_subscription_ = create_subscription<geometry_msgs::msg::PointStamped>(
      "/gps/position", rclcpp::QoS(rclcpp::KeepLast(3)).reliable(),
      std::bind(&PlanarEkfLifecycle::on_gps, this, std::placeholders::_1));

    // 사용자 정의 Service 서버다. 짧은 상태 재설정에는 Service가 맞고, 오래 걸리거나
    // 취소가 필요한 작업이라면 전날 배운 Action이 더 맞다.
    reset_service_ = create_service<daily_robotics_2026_09_06::srv::ResetPose>(
      "/ekf/reset_pose",
      std::bind(
        &PlanarEkfLifecycle::on_reset_request, this,
        std::placeholders::_1, std::placeholders::_2));

    RCLCPP_INFO(get_logger(), "CONFIGURE complete: EKF buffers and ROS entities are ready");
    return CallbackReturn::SUCCESS;
  }

  // activate 전이는 출력 Publisher를 활성화한다. 이 순간부터 센서 콜백이 EKF를 갱신한다.
  CallbackReturn on_activate(const rclcpp_lifecycle::State & previous_state) override
  {
    (void)previous_state;
    pose_publisher_->on_activate();
    RCLCPP_INFO(get_logger(), "ACTIVE: predict/correct and /ekf/pose publication enabled");
    return CallbackReturn::SUCCESS;
  }

  // deactivate는 노드를 파괴하지 않고 출력만 안전하게 멈춰 현장 점검/재튜닝을 허용한다.
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State & previous_state) override
  {
    (void)previous_state;
    pose_publisher_->on_deactivate();
    last_wheel_stamp_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
    RCLCPP_INFO(get_logger(), "INACTIVE: estimator output disabled");
    return CallbackReturn::SUCCESS;
  }

  // cleanup은 configure에서 잡은 엔티티를 해제해 UNCONFIGURED와 같은 상태로 되돌린다.
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State & previous_state) override
  {
    (void)previous_state;
    reset_service_.reset();
    gps_subscription_.reset();
    wheel_subscription_.reset();
    pose_publisher_.reset();
    RCLCPP_INFO(get_logger(), "CLEANUP complete: communication resources released");
    return CallbackReturn::SUCCESS;
  }

private:
  static constexpr std::size_t index(std::size_t row, std::size_t column)
  {
    return row * 3U + column;
  }

  static double wrap_angle(double angle)
  {
    // atan2(sin, cos)는 어떤 입력도 [-pi, pi]로 감싸 yaw 불연속 누적을 막는다.
    return std::atan2(std::sin(angle), std::cos(angle));
  }

  bool is_active() const
  {
    // Lifecycle state ID를 검사해 INACTIVE에서 도착한 센서 데이터가 상태를 바꾸지 않게 한다.
    return get_current_state().id() == lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE;
  }

  void reset_filter(double x, double y, double yaw)
  {
    std::lock_guard<std::mutex> lock(filter_mutex_);
    state_ = {x, y, wrap_angle(yaw)};
    covariance_.fill(0.0);
    // 초기 분산 P0: 위치 표준편차 약 0.71 m, yaw 표준편차 약 0.45 rad를 뜻한다.
    covariance_[index(0, 0)] = 0.50;
    covariance_[index(1, 1)] = 0.50;
    covariance_[index(2, 2)] = 0.20;
  }

  // /ekf/reset_pose 요청을 처리하는 Service 콜백이다. 필터 상태와 P를 원자적으로 재설정한다.
  void on_reset_request(
    const std::shared_ptr<daily_robotics_2026_09_06::srv::ResetPose::Request> request,
    std::shared_ptr<daily_robotics_2026_09_06::srv::ResetPose::Response> response)
  {
    if (!is_active()) {
      response->accepted = false;
      response->message = "EKF is not ACTIVE";
      return;
    }

    reset_filter(request->x, request->y, request->yaw);
    last_wheel_stamp_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
    response->accepted = true;
    response->message = "state and covariance reset";
    RCLCPP_INFO(
      get_logger(), "ResetPose applied: x=%.3f y=%.3f yaw=%.3f",
      request->x, request->y, request->yaw);
  }

  // wheel/gyro 측정으로 비선형 운동 모델을 적분하고 공분산을 전파하는 EKF Predict 단계다.
  void on_wheel(const geometry_msgs::msg::TwistStamped::SharedPtr message)
  {
    if (!is_active()) {
      return;
    }

    const rclcpp::Time current_stamp(message->header.stamp);
    if (last_wheel_stamp_.nanoseconds() == 0) {
      last_wheel_stamp_ = current_stamp;
      return;
    }

    // 비정상 timestamp나 긴 dropout이 한 번의 거대한 적분으로 폭주하지 않도록 상한을 둔다.
    const double raw_dt = (current_stamp - last_wheel_stamp_).seconds();
    last_wheel_stamp_ = current_stamp;
    const double dt = std::clamp(raw_dt, 0.001, 0.20);
    const double velocity = message->twist.linear.x;
    const double yaw_rate = message->twist.angular.z;

    std::lock_guard<std::mutex> lock(filter_mutex_);
    const double yaw = state_[2];

    // 상태식 f(x,u): [x+v cos(yaw)dt, y+v sin(yaw)dt, yaw+omega dt].
    state_[0] += velocity * std::cos(yaw) * dt;
    state_[1] += velocity * std::sin(yaw) * dt;
    state_[2] = wrap_angle(state_[2] + yaw_rate * dt);

    // F = df/dx는 비선형 f를 현재 yaw 주변에서 1차 선형화한 Jacobian이다.
    Matrix3 jacobian_f{1.0, 0.0, -velocity * std::sin(yaw) * dt,
                       0.0, 1.0,  velocity * std::cos(yaw) * dt,
                       0.0, 0.0, 1.0};

    // P^- = F P F^T + Q. 고정 크기 배열은 행렬 크기 오류와 callback 중 heap 할당을 줄인다.
    Matrix3 f_times_p{};
    Matrix3 predicted_covariance{};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t k = 0; k < 3; ++k) {
          f_times_p[index(row, column)] +=
            jacobian_f[index(row, k)] * covariance_[index(k, column)];
        }
      }
    }
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t k = 0; k < 3; ++k) {
          // F^T[k,c] = F[c,k]이므로 아래 인덱스 순서를 사용한다.
          predicted_covariance[index(row, column)] +=
            f_times_p[index(row, k)] * jacobian_f[index(column, k)];
        }
      }
    }

    // Q는 모델/속도 센서 오차를 나타낸다. 시간 간격이 길수록 불확실성이 커지게 dt를 곱한다.
    predicted_covariance[index(0, 0)] += 0.020 * dt;
    predicted_covariance[index(1, 1)] += 0.020 * dt;
    predicted_covariance[index(2, 2)] += 0.010 * dt;
    covariance_ = predicted_covariance;
    publish_pose_locked(current_stamp);
  }

  // GPS x,y를 이용해 innovation, Kalman gain, Joseph covariance를 계산하는 Correct 단계다.
  void on_gps(const geometry_msgs::msg::PointStamped::SharedPtr message)
  {
    if (!is_active()) {
      return;
    }

    std::lock_guard<std::mutex> lock(filter_mutex_);
    constexpr double measurement_variance = 0.04;  // sigma=0.20 m의 제곱이다.

    // z-h(x): 이 예제의 h(x)=[x,y]이므로 residual은 GPS와 예측 위치의 단순 차다.
    const std::array<double, 2> innovation{
      message->point.x - state_[0], message->point.y - state_[1]};

    // S = H P H^T + R. H가 상태의 첫 두 성분만 선택하므로 P의 좌상단 2x2를 쓴다.
    const double s00 = covariance_[index(0, 0)] + measurement_variance;
    const double s01 = covariance_[index(0, 1)];
    const double s10 = covariance_[index(1, 0)];
    const double s11 = covariance_[index(1, 1)] + measurement_variance;
    const double determinant = s00 * s11 - s01 * s10;
    if (determinant <= 1.0e-12) {
      RCLCPP_ERROR(get_logger(), "innovation covariance is singular; GPS update skipped");
      return;
    }

    // 2x2 S^-1 = 1/det(S) [[s11,-s01],[-s10,s00]].
    const std::array<double, 4> inverse_s{
      s11 / determinant, -s01 / determinant,
      -s10 / determinant, s00 / determinant};

    // K = P H^T S^-1. H^T가 첫 두 열을 선택하므로 K는 3x2 고정 배열이면 충분하다.
    std::array<double, 6> kalman_gain{};
    for (std::size_t row = 0; row < 3; ++row) {
      kalman_gain[row * 2U] =
        covariance_[index(row, 0)] * inverse_s[0] +
        covariance_[index(row, 1)] * inverse_s[2];
      kalman_gain[row * 2U + 1U] =
        covariance_[index(row, 0)] * inverse_s[1] +
        covariance_[index(row, 1)] * inverse_s[3];
    }

    // x = x^- + K y. x,y 측정이 yaw도 보정하는 이유는 P의 교차공분산 때문이다.
    for (std::size_t row = 0; row < 3; ++row) {
      state_[row] +=
        kalman_gain[row * 2U] * innovation[0] +
        kalman_gain[row * 2U + 1U] * innovation[1];
    }
    state_[2] = wrap_angle(state_[2]);

    // Joseph form: P=(I-KH)P^-(I-KH)^T + K R K^T.
    // 단순 (I-KH)P보다 연산은 많지만 유한 정밀도에서 대칭/양의 준정부호 보존에 유리하다.
    Matrix3 identity_minus_kh{1.0, 0.0, 0.0,
                              0.0, 1.0, 0.0,
                              0.0, 0.0, 1.0};
    for (std::size_t row = 0; row < 3; ++row) {
      identity_minus_kh[index(row, 0)] -= kalman_gain[row * 2U];
      identity_minus_kh[index(row, 1)] -= kalman_gain[row * 2U + 1U];
    }

    Matrix3 a_times_p{};
    Matrix3 corrected_covariance{};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t k = 0; k < 3; ++k) {
          a_times_p[index(row, column)] +=
            identity_minus_kh[index(row, k)] * covariance_[index(k, column)];
        }
      }
    }
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t k = 0; k < 3; ++k) {
          corrected_covariance[index(row, column)] +=
            a_times_p[index(row, k)] * identity_minus_kh[index(column, k)];
        }
        corrected_covariance[index(row, column)] += measurement_variance * (
          kalman_gain[row * 2U] * kalman_gain[column * 2U] +
          kalman_gain[row * 2U + 1U] * kalman_gain[column * 2U + 1U]);
      }
    }

    // 작은 부동소수점 비대칭을 평균내 이후 역행렬 계산의 수치 품질을 지킨다.
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t column = row + 1U; column < 3; ++column) {
        const double symmetric_value = 0.5 * (
          corrected_covariance[index(row, column)] +
          corrected_covariance[index(column, row)]);
        corrected_covariance[index(row, column)] = symmetric_value;
        corrected_covariance[index(column, row)] = symmetric_value;
      }
    }
    covariance_ = corrected_covariance;
    publish_pose_locked(rclcpp::Time(message->header.stamp));
  }

  // 내부 [x,y,yaw]와 3x3 P를 표준 PoseWithCovarianceStamped 메시지로 바꿔 게시한다.
  void publish_pose_locked(const rclcpp::Time & stamp)
  {
    if (!pose_publisher_ || !pose_publisher_->is_activated()) {
      return;
    }

    geometry_msgs::msg::PoseWithCovarianceStamped output;
    output.header.stamp = stamp;
    output.header.frame_id = "map";
    output.pose.pose.position.x = state_[0];
    output.pose.pose.position.y = state_[1];
    // 평면 yaw를 단위 쿼터니언 q=[0,0,sin(yaw/2),cos(yaw/2)]로 변환한다.
    output.pose.pose.orientation.z = std::sin(0.5 * state_[2]);
    output.pose.pose.orientation.w = std::cos(0.5 * state_[2]);

    // ROS 6x6 순서 [x,y,z,roll,pitch,yaw]에 내부 3x3 [x,y,yaw] P를 삽입한다.
    output.pose.covariance.fill(0.0);
    output.pose.covariance[0] = covariance_[index(0, 0)];
    output.pose.covariance[1] = covariance_[index(0, 1)];
    output.pose.covariance[5] = covariance_[index(0, 2)];
    output.pose.covariance[6] = covariance_[index(1, 0)];
    output.pose.covariance[7] = covariance_[index(1, 1)];
    output.pose.covariance[11] = covariance_[index(1, 2)];
    output.pose.covariance[30] = covariance_[index(2, 0)];
    output.pose.covariance[31] = covariance_[index(2, 1)];
    output.pose.covariance[35] = covariance_[index(2, 2)];
    // 이 평면 필터가 추정하지 않는 z/roll/pitch 축은 큰 분산으로 명시한다.
    output.pose.covariance[14] = 1.0e6;
    output.pose.covariance[21] = 1.0e6;
    output.pose.covariance[28] = 1.0e6;
    pose_publisher_->publish(output);
  }

  rclcpp_lifecycle::LifecyclePublisher<
    geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pose_publisher_;
  rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr wheel_subscription_;
  rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr gps_subscription_;
  rclcpp::Service<daily_robotics_2026_09_06::srv::ResetPose>::SharedPtr reset_service_;

  std::array<double, 3> state_{0.0, 0.0, 0.0};
  Matrix3 covariance_{};
  rclcpp::Time last_wheel_stamp_{0, 0, RCL_ROS_TIME};
  std::mutex filter_mutex_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<PlanarEkfLifecycle>();
  // LifecycleNode는 NodeBaseInterface를 Executor에 등록해야 표준 ChangeState Service도 실행된다.
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node->get_node_base_interface());
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
