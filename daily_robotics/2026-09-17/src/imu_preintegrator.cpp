#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>

#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "std_msgs/msg/u_int32_multi_array.hpp"

using namespace std::chrono_literals;

struct Sample {
  std::int64_t stamp_ns{};
  double ax{};
  double ay{};
  double wz{};
};

// 센서 콜백은 고정 배열에 복사만 하고 적분은 별도 벽시계 타이머에서 한다.
// 이 예제의 SingleThreadedExecutor에서는 콜백끼리 직렬화되어 추가 잠금이 불필요하다.
class ImuPreintegrator final : public rclcpp::Node {
public:
  ImuPreintegrator() : Node("imu_preintegrator") {
    // IMU 발행자와 일치하는 SensorDataQoS는 지연된 과거 샘플보다 최신 샘플을 우선한다.
    imu_sub_ = create_subscription<sensor_msgs::msg::Imu>(
        "/imu/data", rclcpp::SensorDataQoS(),
        [this](sensor_msgs::msg::Imu::ConstSharedPtr msg) { enqueue(*msg); });
    odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("/imu/delta", rclcpp::QoS(10));
    // 결과보다 늦게 붙은 auditor도 마지막 카운터를 읽도록 transient_local을 사용한다.
    stats_pub_ = create_publisher<std_msgs::msg::UInt32MultiArray>(
        "/imu/integration_stats", rclcpp::QoS(1).reliable().transient_local());
    // ROS time 타이머 대신 벽시계로 처리한다: 재생 일시정지/역행에도 큐 처리가 정의된다.
    timer_ = create_wall_timer(20ms, [this]() { drain_bounded(); });
  }

private:
  static constexpr std::size_t kCapacity = 256;
  static constexpr std::size_t kMaxBatch = 16;

  // O(1) 복사·색인만 수행한다. 포화되면 오래된 값을 덮지 않고 명시적으로 계수한다.
  void enqueue(const sensor_msgs::msg::Imu &msg) {
    if (size_ == kCapacity) {
      ++overflow_count_;
      return;
    }
    // rclcpp::Time은 sec/nanosec Header를 정수 ns로 바꿔 부동소수 시간 오차를 피한다.
    const auto stamp_ns = rclcpp::Time(msg.header.stamp, RCL_ROS_TIME).nanoseconds();
    queue_[write_] = Sample{stamp_ns, msg.linear_acceleration.x,
                           msg.linear_acceleration.y, msg.angular_velocity.z};
    write_ = (write_ + 1) % kCapacity;
    ++size_;
  }

  // 재생 시각이 뒤로 가거나 허용 간격을 벗어나면 이전 적분을 새 궤적에 섞지 않는다.
  void reset_state() {
    yaw_ = vx_ = vy_ = px_ = py_ = 0.0;
    previous_ = Sample{};
    has_previous_ = false;
    ++reset_count_;
  }

  // 50 Hz마다 최대 16개 처리한다. 샘플 적분은 고정 횟수 연산이며 힙 할당이 없다.
  void drain_bounded() {
    std::size_t batch = 0;
    bool integrated = false;
    while (size_ > 0 && batch < kMaxBatch) {
      const auto current = queue_[read_];
      read_ = (read_ + 1) % kCapacity;
      --size_;
      ++batch;
      if (!has_previous_) {
        previous_ = current;
        has_previous_ = true;
        continue;
      }
      const auto delta_ns = current.stamp_ns - previous_.stamp_ns;
      if (delta_ns < 0 || delta_ns > 50'000'000LL) {
        reset_state();
        previous_ = current;
        has_previous_ = true;
        continue;
      }
      if (delta_ns == 0) {
        ++duplicate_count_;
        continue;
      }
      const double dt = static_cast<double>(delta_ns) * 1e-9;
      // 중점법: Δθ=ω_mid·dt, a_world=R(θ+Δθ/2)·a_body.
      // Δp=v·dt+1/2 a_world·dt², Δv=a_world·dt이며 초기 자세/속도는 0이다.
      const double wz = 0.5 * (previous_.wz + current.wz);
      const double theta_mid = yaw_ + 0.5 * wz * dt;
      const double ax = 0.5 * (previous_.ax + current.ax);
      const double ay = 0.5 * (previous_.ay + current.ay);
      const double world_ax = std::cos(theta_mid) * ax - std::sin(theta_mid) * ay;
      const double world_ay = std::sin(theta_mid) * ax + std::cos(theta_mid) * ay;
      px_ += vx_ * dt + 0.5 * world_ax * dt * dt;
      py_ += vy_ * dt + 0.5 * world_ay * dt * dt;
      vx_ += world_ax * dt;
      vy_ += world_ay * dt;
      yaw_ += wz * dt;
      previous_ = current;
      integrated = true;
    }
    max_batch_ = std::max(max_batch_, static_cast<std::uint32_t>(batch));
    if (integrated) {
      publish_delta();
    }
    // [시계 점프 초기화, 큐 포화, 중복 타임스탬프, 최대 배치] 계약을 외부에 공개한다.
    std_msgs::msg::UInt32MultiArray stats;
    stats.data = {reset_count_, overflow_count_, duplicate_count_, max_batch_};
    stats_pub_->publish(stats);
  }

  // Odometry pose는 odom 프레임, twist는 child_frame_id=base_link 프레임으로 표현한다.
  void publish_delta() {
    nav_msgs::msg::Odometry out;
    // rclcpp::Time::to_msg()에 의존하지 않고 Jazzy 메시지 필드를 직접 채운다.
    out.header.stamp.sec = static_cast<std::int32_t>(previous_.stamp_ns / 1'000'000'000LL);
    out.header.stamp.nanosec = static_cast<std::uint32_t>(previous_.stamp_ns % 1'000'000'000LL);
    out.header.frame_id = "odom";
    out.child_frame_id = "base_link";
    out.pose.pose.position.x = px_;
    out.pose.pose.position.y = py_;
    out.pose.pose.orientation.z = std::sin(0.5 * yaw_);
    out.pose.pose.orientation.w = std::cos(0.5 * yaw_);
    out.twist.twist.linear.x = std::cos(yaw_) * vx_ + std::sin(yaw_) * vy_;
    out.twist.twist.linear.y = -std::sin(yaw_) * vx_ + std::cos(yaw_) * vy_;
    out.twist.twist.angular.z = previous_.wz;
    // Odometry에는 '미상=-1' 표준 sentinel이 없다. 큰 양의 대각 분산은 임시 보수값일 뿐
    // 실제 잡음 모델/공분산 전파의 대체물이 아니므로 이 출력을 실기 fusion에 연결하지 않는다.
    for (std::size_t axis = 0; axis < 6; ++axis) {
      out.pose.covariance[axis * 6 + axis] = 1e6;
      out.twist.covariance[axis * 6 + axis] = 1e6;
    }
    odom_pub_->publish(out);
  }

  std::array<Sample, kCapacity> queue_{};
  std::size_t read_{0}, write_{0}, size_{0};
  Sample previous_{};
  bool has_previous_{false};
  double yaw_{0.0}, vx_{0.0}, vy_{0.0}, px_{0.0}, py_{0.0};
  std::uint32_t reset_count_{0}, overflow_count_{0}, duplicate_count_{0}, max_batch_{0};
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<std_msgs::msg::UInt32MultiArray>::SharedPtr stats_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  // 단일 executor는 enqueue와 drain_bounded를 동시에 실행하지 않는다.
  rclcpp::spin(std::make_shared<ImuPreintegrator>());
  rclcpp::shutdown();
  return 0;
}
