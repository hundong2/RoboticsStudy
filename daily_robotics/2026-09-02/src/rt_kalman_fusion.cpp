#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/executors/multi_threaded_executor.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace daily_robotics
{

struct PositionMeasurement
{
  std::int64_t stamp_nanoseconds{0};
  double position_meters{0.0};
  double variance{1.0};
};

/// 단일 생산자와 단일 소비자 사이에서 mutex 없이 데이터를 넘기는 고정 크기 링 버퍼다.
/// Capacity-1개의 원소를 보관하며 실행 중 new/delete가 없어 지연 상한을 추론하기 쉽다.
template<typename T, std::size_t Capacity>
class BoundedSpscQueue
{
  static_assert(Capacity >= 2, "SPSC queue capacity must be at least two");

public:
  bool try_push(const T & value) noexcept
  {
    // relaxed는 이 스레드가 소유한 head 읽기에는 순서 보장이 더 필요하지 않음을 뜻한다.
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = increment(head);

    // acquire로 소비자가 공개한 tail 이후의 메모리 변경을 이 스레드에서 관측한다.
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }

    storage_[head] = value;
    // release는 원소 쓰기가 끝난 뒤 새 head가 소비자에게 보이도록 순서를 보장한다.
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool try_pop(T & value) noexcept
  {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    // acquire는 생산자가 release로 공개한 storage 원소를 안전하게 읽게 한다.
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }

    value = storage_[tail];
    // 소비가 끝난 슬롯을 생산자가 다시 사용해도 됨을 release로 공개한다.
    tail_.store(increment(tail), std::memory_order_release);
    return true;
  }

private:
  static constexpr std::size_t increment(std::size_t index) noexcept
  {
    return (index + 1U) % Capacity;
  }

  // alignas(64)는 흔한 CPU cache line 경계에 인덱스를 분리해 false sharing을 줄인다.
  alignas(64) std::array<T, Capacity> storage_{};
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

/// 상태 x=[위치, 속도]^T를 추정하는 1차원 등속도 칼만 필터다.
class ConstantVelocityKalmanFilter
{
public:
  void initialize(double position_meters) noexcept
  {
    position_ = position_meters;
    velocity_ = 0.0;
    p00_ = 1.0;
    p01_ = 0.0;
    p10_ = 0.0;
    p11_ = 1.0;
    initialized_ = true;
  }

  void predict(double dt_seconds, double acceleration_variance) noexcept
  {
    // 수식 연결: x_k^- = F x_(k-1), F=[[1,dt],[0,1]].
    position_ += dt_seconds * velocity_;

    // 백색 가속도 잡음을 위치/속도 공분산으로 적분한 Q 행렬이다.
    const double dt2 = dt_seconds * dt_seconds;
    const double dt3 = dt2 * dt_seconds;
    const double dt4 = dt2 * dt2;
    const double q00 = 0.25 * dt4 * acceleration_variance;
    const double q01 = 0.5 * dt3 * acceleration_variance;
    const double q11 = dt2 * acceleration_variance;

    // 수식 연결: P_k^- = F P_(k-1) F^T + Q를 2x2 스칼라 연산으로 펼쳤다.
    const double next_p00 = p00_ + dt_seconds * (p10_ + p01_) + dt2 * p11_ + q00;
    const double next_p01 = p01_ + dt_seconds * p11_ + q01;
    const double next_p10 = p10_ + dt_seconds * p11_ + q01;
    const double next_p11 = p11_ + q11;
    p00_ = next_p00;
    p01_ = next_p01;
    p10_ = next_p10;
    p11_ = next_p11;
  }

  void correct(double measured_position, double measurement_variance) noexcept
  {
    // H=[1,0]이므로 innovation y=z-Hx는 측정 위치와 예측 위치의 차이다.
    const double innovation = measured_position - position_;
    const double innovation_variance = p00_ + measurement_variance;
    const double gain_position = p00_ / innovation_variance;
    const double gain_velocity = p10_ / innovation_variance;

    // 수식 연결: x_k = x_k^- + K*y. 위치 오차가 속도 추정도 함께 보정한다.
    position_ += gain_position * innovation;
    velocity_ += gain_velocity * innovation;

    // Joseph form P=(I-KH)P(I-KH)^T+KRK^T는 반올림 오차에도 P를 양의 준정부호로 유지한다.
    const double a00 = 1.0 - gain_position;
    const double a10 = -gain_velocity;
    const double ap00 = a00 * p00_;
    const double ap01 = a00 * p01_;
    const double ap10 = a10 * p00_ + p10_;
    const double ap11 = a10 * p01_ + p11_;

    const double next_p00 = ap00 * a00 + gain_position * gain_position * measurement_variance;
    const double next_p01 = ap00 * a10 + ap01 +
      gain_position * gain_velocity * measurement_variance;
    const double next_p10 = ap10 * a00 + gain_velocity * gain_position * measurement_variance;
    const double next_p11 = ap10 * a10 + ap11 +
      gain_velocity * gain_velocity * measurement_variance;

    p00_ = next_p00;
    // 수치 오차로 비대칭이 생기지 않도록 P01과 P10의 평균을 취한다.
    p01_ = 0.5 * (next_p01 + next_p10);
    p10_ = p01_;
    p11_ = next_p11;
  }

  bool initialized() const noexcept {return initialized_;}
  double position() const noexcept {return position_;}
  double velocity() const noexcept {return velocity_;}
  double position_variance() const noexcept {return p00_;}
  double velocity_variance() const noexcept {return p11_;}

private:
  bool initialized_{false};
  double position_{0.0};
  double velocity_{0.0};
  double p00_{1.0};
  double p01_{0.0};
  double p10_{0.0};
  double p11_{1.0};
};

/// ROS 수신 콜백과 필터 계산 콜백을 분리하고 SPSC 큐로 연결하는 상태 추정 노드다.
class RtKalmanFusionNode final : public rclcpp::Node
{
public:
  RtKalmanFusionNode()
  : Node("rt_kalman_fusion"),
    acceleration_variance_(declare_parameter<double>("acceleration_variance", 2.0)),
    fallback_measurement_variance_(
      declare_parameter<double>("fallback_measurement_variance", 0.1225))
  {
    // 서로 다른 callback group은 MultiThreadedExecutor가 수신과 계산을 병렬 실행할 수 있게 한다.
    // 각 그룹은 MutuallyExclusive라 같은 종류의 콜백 자체가 중첩 실행되지는 않는다.
    producer_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    consumer_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions subscription_options;
    subscription_options.callback_group = producer_group_;

    // 센서 QoS는 최신성 우선(best effort, 작은 depth)이라 밀린 과거 데이터를 처리하지 않는다.
    const auto sensor_qos = rclcpp::SensorDataQoS().keep_last(5);
    measurement_subscription_ = create_subscription<nav_msgs::msg::Odometry>(
      "/sensors/noisy_odometry",
      sensor_qos,
      std::bind(&RtKalmanFusionNode::enqueue_measurement, this, std::placeholders::_1),
      subscription_options);

    // 추정 결과는 reliable로 발행해 시각화/제어 노드와의 전달 신뢰도를 우선한다.
    estimate_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/state_estimation/filtered_odometry",
      rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    // 5 ms 소비 주기는 100 Hz 입력보다 빠르므로 정상 부하에서는 큐가 빠르게 비워진다.
    // callback group을 명시하여 이 타이머가 생산자 콜백과 다른 executor worker에서 실행될 수 있다.
    processing_timer_ = create_wall_timer(
      5ms, std::bind(&RtKalmanFusionNode::process_measurements, this), consumer_group_);
  }

private:
  /// DDS 메시지를 필터용 고정 크기 값 객체로 복사해 비차단 큐에 넣는 생산자 콜백이다.
  void enqueue_measurement(const nav_msgs::msg::Odometry::SharedPtr message)
  {
    PositionMeasurement measurement;
    // rclcpp::Time은 sec/nanosec 조합을 정수 나노초로 바꿔 시간 차 계산의 정밀도를 지킨다.
    measurement.stamp_nanoseconds = rclcpp::Time(message->header.stamp).nanoseconds();
    measurement.position_meters = message->pose.pose.position.x;
    const double reported_variance = message->pose.covariance[0];
    measurement.variance = reported_variance > 0.0 ?
      reported_variance : fallback_measurement_variance_;

    if (!measurement_queue_.try_push(measurement)) {
      // 큐가 가득 차면 기다리지 않고 최신 입력을 버린다. 제어 경로의 무한 대기보다 예측 가능하다.
      dropped_measurements_.fetch_add(1U, std::memory_order_relaxed);
    }
  }

  /// 큐의 센서 샘플을 모두 소비해 predict/correct를 수행하고 필터링된 odometry를 발행한다.
  void process_measurements()
  {
    PositionMeasurement measurement;
    while (measurement_queue_.try_pop(measurement)) {
      if (!filter_.initialized()) {
        filter_.initialize(measurement.position_meters);
        previous_stamp_nanoseconds_ = measurement.stamp_nanoseconds;
      } else {
        const double raw_dt = static_cast<double>(
          measurement.stamp_nanoseconds - previous_stamp_nanoseconds_) * 1.0e-9;
        // 비정상 타임스탬프가 공분산을 폭발시키지 않게 모델 유효 범위 [0.1 ms, 100 ms]로 제한한다.
        const double dt_seconds = std::clamp(raw_dt, 1.0e-4, 0.1);
        filter_.predict(dt_seconds, acceleration_variance_);
        filter_.correct(measurement.position_meters, measurement.variance);
        previous_stamp_nanoseconds_ = measurement.stamp_nanoseconds;
      }

      publish_estimate(measurement.stamp_nanoseconds);
    }
  }

  /// 칼만 필터 상태 x=[position, velocity]와 공분산을 nav_msgs/Odometry로 변환한다.
  void publish_estimate(std::int64_t stamp_nanoseconds)
  {
    nav_msgs::msg::Odometry estimate;
    // rclcpp::Time의 변환 연산자가 정수 나노초를 ROS builtin_interfaces/Time으로 옮긴다.
    estimate.header.stamp = rclcpp::Time(stamp_nanoseconds);
    estimate.header.frame_id = "map";
    estimate.child_frame_id = "base_link_estimated";
    estimate.pose.pose.position.x = filter_.position();
    estimate.pose.pose.orientation.w = 1.0;
    estimate.pose.covariance[0] = filter_.position_variance();
    estimate.twist.twist.linear.x = filter_.velocity();
    estimate.twist.covariance[0] = filter_.velocity_variance();
    estimate_publisher_->publish(estimate);
  }

  static constexpr std::size_t kQueueCapacity = 256;

  const double acceleration_variance_;
  const double fallback_measurement_variance_;
  BoundedSpscQueue<PositionMeasurement, kQueueCapacity> measurement_queue_;
  ConstantVelocityKalmanFilter filter_;
  std::int64_t previous_stamp_nanoseconds_{0};
  std::atomic<std::uint64_t> dropped_measurements_{0};
  rclcpp::CallbackGroup::SharedPtr producer_group_;
  rclcpp::CallbackGroup::SharedPtr consumer_group_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr measurement_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr estimate_publisher_;
  rclcpp::TimerBase::SharedPtr processing_timer_;
};

}  // namespace daily_robotics

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<daily_robotics::RtKalmanFusionNode>();

  // 두 worker thread는 생산자/소비자 callback group을 병렬로 처리한다.
  // 이것만으로 hard RT가 되지는 않으며 PREEMPT_RT, 우선순위, affinity, 메모리 잠금 검증이 추가로 필요하다.
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2U);
  executor.add_node(node);
  executor.spin();

  rclcpp::shutdown();
  return 0;
}
