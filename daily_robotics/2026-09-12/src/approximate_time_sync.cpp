#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <sstream>

#include "message_filters/subscriber.h"
#include "message_filters/synchronizer.h"
#include "message_filters/sync_policies/approximate_time.h"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/channel_float32.hpp"
#include "sensor_msgs/msg/point_cloud.hpp"
#include "std_msgs/msg/string.hpp"

namespace daily_robotics
{

constexpr std::size_t kMaxFeatures = 32U;
constexpr std::int64_t kMaxSkewNs = 20'000'000LL;
constexpr double kMinBearingDot = 0.995;

// 이 노드는 camera와 LiDAR의 Header stamp를 ApproximateTime으로 짝지은 뒤,
// feature ID와 bearing 일치성을 검사해 metric correspondence만 VO에 전달한다.
class ApproximateTimeSync final : public rclcpp::Node
{
public:
  using Cloud = sensor_msgs::msg::PointCloud;
  // ApproximateTime은 stamp가 완전히 같지 않아도 queue 안에서 가장 그럴듯한 두 메시지를 선택한다.
  using SyncPolicy = message_filters::sync_policies::ApproximateTime<Cloud, Cloud>;

  ApproximateTimeSync()
  : Node("approximate_time_sync")
  {
    const auto sensor_qos = rclcpp::SensorDataQoS();
    metric_pub_ = create_publisher<Cloud>("/sync/metric_features", sensor_qos);
    diagnostics_pub_ = create_publisher<std_msgs::msg::String>(
      "/sync/diagnostics", rclcpp::QoS(10).reliable());

    // message_filters::Subscriber는 rclcpp Subscription을 filter chain에 연결하는 adapter다.
    // Publisher와 같은 rmw QoS profile을 넘겨 best-effort/reliability 불일치를 피한다.
    camera_sub_.subscribe(this, "/camera/features", sensor_qos.get_rmw_qos_profile());
    lidar_sub_.subscribe(this, "/lidar/features", sensor_qos.get_rmw_qos_profile());

    // queue 10은 한쪽 센서가 잠깐 앞서도 후보를 보존하되 메모리와 지연이 무한히 자라지 않게 한다.
    sync_ = std::make_shared<message_filters::Synchronizer<SyncPolicy>>(
      SyncPolicy(10U), camera_sub_, lidar_sub_);
    // 후보 pair 자체가 20 ms보다 멀면 Synchronizer 단계에서 선택하지 않는다.
    sync_->setMaxIntervalDuration(rclcpp::Duration::from_nanoseconds(kMaxSkewNs));
    // Age penalty은 오래된 후보보다 새 후보를 선호하는 무차원 cost 가중치다.
    sync_->setAgePenalty(0.10);
    sync_->registerCallback(
      std::bind(&ApproximateTimeSync::on_pair, this, std::placeholders::_1, std::placeholders::_2));
  }

private:
  // PointCloud channel에서 feature_id 위치를 찾는다. sensor driver마다 channel 순서가 다를 수 있다.
  static const sensor_msgs::msg::ChannelFloat32 * find_id_channel(const Cloud & cloud)
  {
    for (const auto & channel : cloud.channels) {
      if (channel.name == "feature_id") {
        return &channel;
      }
    }
    return nullptr;
  }

  // 두 센서가 같은 landmark를 가리키는지 cos(angle)=b_camera·normalize(p_lidar)로 검사한다.
  static double bearing_dot(
    const geometry_msgs::msg::Point32 & bearing,
    const geometry_msgs::msg::Point32 & metric)
  {
    const double metric_norm = std::hypot(metric.x, metric.y);
    const double bearing_norm = std::hypot(bearing.x, bearing.y);
    if (metric_norm < 1.0e-9 || bearing_norm < 1.0e-9) {
      return -1.0;
    }
    return (static_cast<double>(bearing.x) * static_cast<double>(metric.x) +
      static_cast<double>(bearing.y) * static_cast<double>(metric.y)) /
      (bearing_norm * metric_norm);
  }

  // Synchronizer가 고른 pair를 다시 검증하는 callback이다. 정책 설정과 안전 계약을 분리해
  // 나중에 queue/penalty가 바뀌어도 20 ms 초과 pair가 알고리즘에 들어가지 않게 한다.
  void on_pair(const Cloud::ConstSharedPtr & camera, const Cloud::ConstSharedPtr & lidar)
  {
    const rclcpp::Time camera_stamp(camera->header.stamp);
    const rclcpp::Time lidar_stamp(lidar->header.stamp);
    const std::int64_t skew_ns = std::llabs((lidar_stamp - camera_stamp).nanoseconds());
    if (skew_ns > kMaxSkewNs) {
      ++rejected_pairs_;
      RCLCPP_WARN(
        get_logger(), "동기화 계약 위반: skew=%lld us",
        static_cast<long long>(skew_ns / 1000LL));
      return;
    }

    const auto * camera_ids = find_id_channel(*camera);
    const auto * lidar_ids = find_id_channel(*lidar);
    if (camera_ids == nullptr || lidar_ids == nullptr) {
      ++rejected_pairs_;
      RCLCPP_WARN(get_logger(), "feature_id channel이 없어 pair를 거부합니다");
      return;
    }

    const std::size_t candidate_count = std::min(
      {camera->points.size(), lidar->points.size(), camera_ids->values.size(),
        lidar_ids->values.size(), kMaxFeatures});

    Cloud output;
    output.header.stamp = lidar->header.stamp;
    output.header.frame_id = lidar->header.frame_id;
    // ROS sequence는 동적 할당을 사용한다. reserve로 재할당 횟수는 줄지만 hard RT를 보장하지는 않는다.
    output.points.reserve(candidate_count);
    output.channels.resize(2U);
    output.channels[0].name = "feature_id";
    output.channels[1].name = "bearing_dot";
    output.channels[0].values.reserve(candidate_count);
    output.channels[1].values.reserve(candidate_count);

    std::size_t rejected_features = 0U;
    for (std::size_t i = 0; i < candidate_count; ++i) {
      // Simulator는 같은 순서를 쓰지만, ID 비교를 강제해 driver/matcher contract 파손을 즉시 드러낸다.
      const auto camera_id = static_cast<std::uint32_t>(std::lround(camera_ids->values[i]));
      const auto lidar_id = static_cast<std::uint32_t>(std::lround(lidar_ids->values[i]));
      const double dot = bearing_dot(camera->points[i], lidar->points[i]);
      if (camera_id != lidar_id || dot < kMinBearingDot) {
        ++rejected_features;
        continue;
      }
      output.points.push_back(lidar->points[i]);
      output.channels[0].values.push_back(static_cast<float>(lidar_id));
      output.channels[1].values.push_back(static_cast<float>(dot));
    }

    if (output.points.size() < 3U) {
      ++rejected_pairs_;
      RCLCPP_WARN(get_logger(), "유효 correspondence가 %zu개뿐이라 pair를 거부합니다", output.points.size());
      return;
    }

    metric_pub_->publish(output);
    ++accepted_pairs_;

    if (accepted_pairs_ % 20U == 0U || rejected_features > 0U) {
      std_msgs::msg::String diagnostics;
      std::ostringstream stream;
      stream << "paired=" << accepted_pairs_
             << " pair_rejected=" << rejected_pairs_
             << " skew_us=" << (skew_ns / 1000LL)
             << " accepted_features=" << output.points.size()
             << " bearing_rejected=" << rejected_features;
      diagnostics.data = stream.str();
      diagnostics_pub_->publish(diagnostics);
    }
  }

  message_filters::Subscriber<Cloud> camera_sub_;
  message_filters::Subscriber<Cloud> lidar_sub_;
  std::shared_ptr<message_filters::Synchronizer<SyncPolicy>> sync_;
  rclcpp::Publisher<Cloud>::SharedPtr metric_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diagnostics_pub_;
  std::uint64_t accepted_pairs_{0U};
  std::uint64_t rejected_pairs_{0U};
};

}  // namespace daily_robotics

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  const auto node = std::make_shared<daily_robotics::ApproximateTimeSync>();
  // message_filters callback도 결국 rclcpp executor가 subscription callback을 dispatch해야 실행된다.
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
