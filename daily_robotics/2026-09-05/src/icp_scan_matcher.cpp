#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>

#include "geometry_msgs/msg/pose2_d.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

/** 2D 점 하나를 x/y 두 실수로만 보관하는 고정 크기 알고리즘 내부 타입이다. */
struct Point2
{
  double x{0.0};
  double y{0.0};
};

/** 현재 scan 좌표를 이전 scan 좌표로 옮기는 SE(2) 변환과 품질 지표다. */
struct Transform2
{
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
  double rmse{std::numeric_limits<double>::infinity()};
  std::size_t correspondences{0};
};

/**
 * 연속 LaserScan을 고정 배열로 변환하고 point-to-point ICP를 최대 8회 수행하는 노드다.
 * 출력 Pose2D는 "현재 scan frame → 이전 scan frame" 상대 변환이다.
 */
class IcpScanMatcher : public rclcpp::Node
{
public:
  IcpScanMatcher()
  : Node("icp_scan_matcher")
  {
    // 입력과 동일한 SensorDataQoS(best-effort, volatile)여야 DDS QoS 호환성이 성립한다.
    scan_subscription_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SensorDataQoS().keep_last(1),
      std::bind(&IcpScanMatcher::scan_callback, this, std::placeholders::_1));

    // ICP 출력은 상태 추정 결과이므로 reliable KeepLast(1)로 최신 상대 자세를 전달한다.
    pose_publisher_ = this->create_publisher<geometry_msgs::msg::Pose2D>(
      "/scan_match/pose", rclcpp::QoS(1).reliable());
  }

private:
  static constexpr std::size_t kMaxPoints = 720;
  static constexpr std::size_t kMaxIterations = 8;
  static constexpr double kMaxCorrespondenceDistance = 0.35;

  /** LaserScan의 유효 polar 표본 (r,θ)을 Cartesian 점 (r cosθ, r sinθ)으로 바꾼다. */
  static std::size_t scan_to_points(
    const sensor_msgs::msg::LaserScan & scan,
    std::array<Point2, kMaxPoints> & output)
  {
    const std::size_t sample_count = std::min(scan.ranges.size(), kMaxPoints);
    std::size_t output_count = 0;
    for (std::size_t index = 0; index < sample_count; ++index) {
      const double range = static_cast<double>(scan.ranges[index]);
      if (!std::isfinite(range) || range < scan.range_min || range > scan.range_max) {
        continue;
      }
      const double angle = static_cast<double>(scan.angle_min) +
        static_cast<double>(index) * static_cast<double>(scan.angle_increment);
      // x=r cosθ, y=r sinθ는 라이다의 극좌표 표본을 2D Euclidean 점으로 바꾸는 식이다.
      output[output_count++] = Point2{range * std::cos(angle), range * std::sin(angle)};
    }
    return output_count;
  }

  /** SE(2) 변환 p'=R(θ)p+t를 점 하나에 적용한다. */
  static Point2 transform_point(const Point2 & point, const Transform2 & transform)
  {
    const double cosine = std::cos(transform.yaw);
    const double sine = std::sin(transform.yaw);
    return Point2{
      cosine * point.x - sine * point.y + transform.x,
      sine * point.x + cosine * point.y + transform.y};
  }

  /**
   * 현재 점 집합을 이전 점 집합에 맞추는 ICP다.
   * 각 반복은 (1) 최근접점 대응, (2) 2D 최소제곱 SE(2), (3) 수렴 판정으로 구성된다.
   */
  Transform2 estimate_current_to_previous(
    const std::array<Point2, kMaxPoints> & current,
    const std::size_t current_count)
  {
    Transform2 estimate;
    constexpr double maximum_distance_squared =
      kMaxCorrespondenceDistance * kMaxCorrespondenceDistance;

    for (std::size_t iteration = 0; iteration < kMaxIterations; ++iteration) {
      std::size_t pair_count = 0;

      for (std::size_t current_index = 0; current_index < current_count; ++current_index) {
        // 현재까지의 추정치를 먼저 적용한 뒤 이전 scan에서 가장 가까운 점을 찾는다.
        const Point2 transformed = transform_point(current[current_index], estimate);
        double best_distance_squared = maximum_distance_squared;
        std::size_t best_previous_index = previous_count_;

        for (std::size_t previous_index = 0; previous_index < previous_count_; ++previous_index) {
          const double dx = previous_points_[previous_index].x - transformed.x;
          const double dy = previous_points_[previous_index].y - transformed.y;
          const double distance_squared = dx * dx + dy * dy;
          if (distance_squared < best_distance_squared) {
            best_distance_squared = distance_squared;
            best_previous_index = previous_index;
          }
        }

        if (best_previous_index < previous_count_) {
          // 대응쌍 배열도 상한 kMaxPoints를 가져 반복 중 heap 할당이 발생하지 않는다.
          correspondence_current_[pair_count] = current[current_index];
          correspondence_previous_[pair_count] = previous_points_[best_previous_index];
          ++pair_count;
        }
      }

      if (pair_count < 8) {
        estimate.correspondences = pair_count;
        return estimate;
      }

      Point2 current_centroid;
      Point2 previous_centroid;
      for (std::size_t index = 0; index < pair_count; ++index) {
        current_centroid.x += correspondence_current_[index].x;
        current_centroid.y += correspondence_current_[index].y;
        previous_centroid.x += correspondence_previous_[index].x;
        previous_centroid.y += correspondence_previous_[index].y;
      }
      const double inverse_count = 1.0 / static_cast<double>(pair_count);
      current_centroid.x *= inverse_count;
      current_centroid.y *= inverse_count;
      previous_centroid.x *= inverse_count;
      previous_centroid.y *= inverse_count;

      double cosine_term = 0.0;
      double sine_term = 0.0;
      for (std::size_t index = 0; index < pair_count; ++index) {
        const double qx = correspondence_current_[index].x - current_centroid.x;
        const double qy = correspondence_current_[index].y - current_centroid.y;
        const double px = correspondence_previous_[index].x - previous_centroid.x;
        const double py = correspondence_previous_[index].y - previous_centroid.y;
        // θ*=atan2(Σ(qx·py-qy·px), Σ(qx·px+qy·py))는 2D Procrustes 회전의 닫힌형 해다.
        cosine_term += qx * px + qy * py;
        sine_term += qx * py - qy * px;
      }

      Transform2 updated;
      updated.yaw = std::atan2(sine_term, cosine_term);
      const double cosine = std::cos(updated.yaw);
      const double sine = std::sin(updated.yaw);
      // t*=p̄-Rq̄: 두 점군의 중심이 회전 뒤 일치하도록 translation을 정한다.
      updated.x = previous_centroid.x -
        (cosine * current_centroid.x - sine * current_centroid.y);
      updated.y = previous_centroid.y -
        (sine * current_centroid.x + cosine * current_centroid.y);
      updated.correspondences = pair_count;

      double squared_error_sum = 0.0;
      for (std::size_t index = 0; index < pair_count; ++index) {
        const Point2 aligned = transform_point(correspondence_current_[index], updated);
        const double dx = aligned.x - correspondence_previous_[index].x;
        const double dy = aligned.y - correspondence_previous_[index].y;
        squared_error_sum += dx * dx + dy * dy;
      }
      // RMSE=sqrt(Σ||p_i-(Rq_i+t)||²/N)는 대응쌍 정합 오차를 m 단위로 보여준다.
      updated.rmse = std::sqrt(squared_error_sum * inverse_count);

      const double translation_delta = std::hypot(updated.x - estimate.x, updated.y - estimate.y);
      const double rotation_delta = std::abs(updated.yaw - estimate.yaw);
      estimate = updated;
      if (translation_delta < 1.0e-5 && rotation_delta < 1.0e-5) {
        break;
      }
    }
    return estimate;
  }

  /** 첫 scan은 기준으로 저장하고, 이후 scan마다 ICP 상대 자세를 계산·게시하는 ROS 콜백이다. */
  void scan_callback(const sensor_msgs::msg::LaserScan::SharedPtr scan)
  {
    std::array<Point2, kMaxPoints> current_points{};
    const std::size_t current_count = scan_to_points(*scan, current_points);
    if (current_count < 8) {
      // throttle은 같은 경고를 최대 2초에 한 번만 출력해 로그 폭주와 timing 교란을 줄인다.
      RCLCPP_WARN_THROTTLE(
        this->get_logger(), *this->get_clock(), 2000,
        "Too few valid scan points: %zu", current_count);
      return;
    }

    if (previous_count_ > 0) {
      const Transform2 estimate = estimate_current_to_previous(current_points, current_count);
      if (estimate.correspondences >= 8 && std::isfinite(estimate.rmse)) {
        geometry_msgs::msg::Pose2D pose;
        pose.x = estimate.x;
        pose.y = estimate.y;
        pose.theta = estimate.yaw;
        pose_publisher_->publish(pose);

        RCLCPP_INFO_THROTTLE(
          this->get_logger(), *this->get_clock(), 1000,
          "ICP current->previous: x=%+.4f y=%+.4f yaw=%+.4f rad, pairs=%zu rmse=%.4f m",
          estimate.x, estimate.y, estimate.yaw, estimate.correspondences, estimate.rmse);
      }
    }

    // 다음 콜백에서 현재 scan이 기준이 된다. std::array 대입은 고정 크기 복사이며 heap을 사용하지 않는다.
    previous_points_ = current_points;
    previous_count_ = current_count;
  }

  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_subscription_;
  rclcpp::Publisher<geometry_msgs::msg::Pose2D>::SharedPtr pose_publisher_;

  std::array<Point2, kMaxPoints> previous_points_{};
  std::size_t previous_count_{0};
  std::array<Point2, kMaxPoints> correspondence_current_{};
  std::array<Point2, kMaxPoints> correspondence_previous_{};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  // SingleThreadedExecutor 기반 spin이 scan callback을 직렬 실행하므로 고정 배열 접근에 lock이 필요 없다.
  rclcpp::spin(std::make_shared<IcpScanMatcher>());
  rclcpp::shutdown();
  return 0;
}
