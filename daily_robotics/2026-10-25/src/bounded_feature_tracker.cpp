#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#include "daily_robotics_2026_10_25/msg/feature_track_stats.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/image.hpp"

using namespace std::chrono_literals;

namespace
{
constexpr std::size_t kWidth = 96U;
constexpr std::size_t kHeight = 72U;
constexpr std::size_t kPixels = kWidth * kHeight;
constexpr std::size_t kFrameQueueSlots = 4U;
constexpr std::size_t kMaxCandidates = 192U;
constexpr std::size_t kMaxFeatures = 48U;
constexpr int kPatchRadius = 2;
constexpr int kSearchRadius = 4;
constexpr double kKnownPlaneDepthM = 4.0;

// 한 producer(image callback)와 한 consumer(worker timer)만 쓰는 고정 크기 ring이다.
// acquire/release는 producer가 payload를 쓴 뒤 head를 공개하고 consumer가 완성된 payload만 보게 한다.
template<typename T, std::size_t Capacity>
class SpscRing
{
  static_assert(Capacity >= 2U, "ring needs a sentinel slot");

public:
  bool push(T value)
  {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = (head + 1U) % Capacity;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    storage_[head] = std::move(value);
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T & value)
  {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    value = std::move(storage_[tail]);
    storage_[tail].reset();
    tail_.store((tail + 1U) % Capacity, std::memory_order_release);
    return true;
  }

private:
  std::array<T, Capacity> storage_{};
  std::atomic<std::size_t> head_{0U};
  std::atomic<std::size_t> tail_{0U};
};

struct Candidate
{
  int x{0};
  int y{0};
  double minimum_eigenvalue{0.0};
};

struct Track
{
  int previous_x{0};
  int previous_y{0};
  int current_x{0};
  int current_y{0};
  double residual{0.0};
};

std::size_t pixel_index(const int x, const int y)
{
  return static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x);
}

bool patch_is_inside(const int x, const int y)
{
  return x >= kPatchRadius && y >= kPatchRadius &&
         x < static_cast<int>(kWidth) - kPatchRadius &&
         y < static_cast<int>(kHeight) - kPatchRadius;
}

// 두 5x5 patch의 L1 photometric residual을 계산한다. 반복 수는 항상 25회다.
int patch_sad(
  const std::array<std::uint8_t, kPixels> & first, const int first_x, const int first_y,
  const std::array<std::uint8_t, kPixels> & second, const int second_x, const int second_y)
{
  int sum = 0;
  for (int dy = -kPatchRadius; dy <= kPatchRadius; ++dy) {
    for (int dx = -kPatchRadius; dx <= kPatchRadius; ++dx) {
      const int first_value = static_cast<int>(first[pixel_index(first_x + dx, first_y + dy)]);
      const int second_value = static_cast<int>(second[pixel_index(second_x + dx, second_y + dy)]);
      sum += std::abs(first_value - second_value);
    }
  }
  return sum;
}
}  // namespace

class BoundedFeatureTracker final : public rclcpp::Node
{
public:
  BoundedFeatureTracker()
  : Node("bounded_feature_tracker")
  {
    // image callback과 numerical worker가 서로 다른 executor thread에서 실행 가능하도록 group을 분리한다.
    // group 분리는 동시성 경계일 뿐 OS/DDS까지 포함하는 hard real-time 보장은 아니다.
    input_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    worker_group_ = create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions input_options;
    input_options.callback_group = input_group_;
    image_subscription_ = create_subscription<sensor_msgs::msg::Image>(
      "/camera/image_rect", rclcpp::SensorDataQoS(),
      std::bind(&BoundedFeatureTracker::on_image, this, std::placeholders::_1), input_options);

    // CameraInfo는 latched calibration이므로 publisher와 같은 Reliable/Transient Local QoS를 요청한다.
    rclcpp::SubscriptionOptions info_options;
    info_options.callback_group = input_group_;
    const auto info_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
    info_subscription_ = create_subscription<sensor_msgs::msg::CameraInfo>(
      "/camera/camera_info", info_qos,
      std::bind(&BoundedFeatureTracker::on_camera_info, this, std::placeholders::_1), info_options);

    odometry_publisher_ = create_publisher<nav_msgs::msg::Odometry>(
      "/visual_frontend/odometry", rclcpp::QoS(10).reliable());
    stats_publisher_ = create_publisher<daily_robotics_2026_10_25::msg::FeatureTrackStats>(
      "/visual_frontend/stats", rclcpp::QoS(10).reliable());

    // 100 Hz worker는 한 wakeup에 image를 최대 하나만 꺼낸다. backlog를 한 번에 비우지 않아
    // callback당 계산량은 bounded지만, overload 시 queue drop이 명시적으로 계측된다.
    worker_timer_ = create_wall_timer(
      10ms, std::bind(&BoundedFeatureTracker::process_one_frame, this), worker_group_);

    RCLCPP_INFO(
      get_logger(),
      "bounded frontend: <=%zu candidates, <=%zu tracks, 5x5 patch, +/- %d px search",
      kMaxCandidates, kMaxFeatures, kSearchRadius);
  }

private:
  using ImageConstPtr = sensor_msgs::msg::Image::ConstSharedPtr;

  // DDS callback에서는 image를 복사/처리하지 않고 shared pointer만 fixed ring으로 넘긴다.
  void on_image(ImageConstPtr image)
  {
    if (!frame_queue_.push(std::move(image))) {
      frame_queue_drops_.fetch_add(1U, std::memory_order_relaxed);
    }
  }

  // calibration callback은 크기, optical frame, focal length 계약을 검증한 뒤 release-store로 공개한다.
  void on_camera_info(const sensor_msgs::msg::CameraInfo::ConstSharedPtr info)
  {
    const bool valid = info->width == kWidth && info->height == kHeight &&
      info->header.frame_id == "camera_optical_frame" && info->k[0] > 1.0 && info->k[4] > 1.0;
    if (valid) {
      focal_length_px_.store(info->k[0], std::memory_order_relaxed);
    }
    camera_model_valid_.store(valid, std::memory_order_release);
  }

  // Shi-Tomasi의 min(lambda1, lambda2)를 3x3 gradient window에서 계산한다.
  // 두 eigenvalue가 모두 클 때만 x/y 양방향으로 구별되는, tracking하기 좋은 점이다.
  double shi_tomasi_score(const std::array<std::uint8_t, kPixels> & image, const int x, const int y)
  {
    double gxx = 0.0;
    double gxy = 0.0;
    double gyy = 0.0;
    for (int wy = -1; wy <= 1; ++wy) {
      for (int wx = -1; wx <= 1; ++wx) {
        const int center_x = x + wx;
        const int center_y = y + wy;
        const double ix = 0.5 * static_cast<double>(
          static_cast<int>(image[pixel_index(center_x + 1, center_y)]) -
          static_cast<int>(image[pixel_index(center_x - 1, center_y)]));
        const double iy = 0.5 * static_cast<double>(
          static_cast<int>(image[pixel_index(center_x, center_y + 1)]) -
          static_cast<int>(image[pixel_index(center_x, center_y - 1)]));
        gxx += ix * ix;
        gxy += ix * iy;
        gyy += iy * iy;
      }
    }
    // 2x2 대칭행렬 G=[gxx gxy; gxy gyy]의 작은 고윳값 closed form이다.
    const double trace = gxx + gyy;
    const double discriminant = std::sqrt((gxx - gyy) * (gxx - gyy) + 4.0 * gxy * gxy);
    return 0.5 * (trace - discriminant);
  }

  // grid sampling으로 후보 수를 제한하고, score 순 greedy NMS로 최대 48개를 고른다.
  std::size_t select_features(
    const std::array<std::uint8_t, kPixels> & image,
    std::uint32_t & candidate_count, double & minimum_selected_eigenvalue)
  {
    std::size_t count = 0U;
    for (int y = 7; y < static_cast<int>(kHeight) - 7 && count < kMaxCandidates; y += 4) {
      for (int x = 7; x < static_cast<int>(kWidth) - 7 && count < kMaxCandidates; x += 4) {
        const double score = shi_tomasi_score(image, x, y);
        if (score >= 1200.0) {
          candidates_[count] = Candidate{x, y, score};
          ++count;
        }
      }
    }
    candidate_count = static_cast<std::uint32_t>(count);

    // std::sort의 대상은 fixed array 앞쪽 최대 192개뿐이다. 비교 횟수는 입력 cap으로 제한된다.
    std::sort(
      candidates_.begin(), candidates_.begin() + static_cast<std::ptrdiff_t>(count),
      [](const Candidate & left, const Candidate & right) {
        return left.minimum_eigenvalue > right.minimum_eigenvalue;
      });

    std::size_t selected_count = 0U;
    minimum_selected_eigenvalue = 0.0;
    for (std::size_t i = 0U; i < count && selected_count < kMaxFeatures; ++i) {
      bool spatially_separated = true;
      for (std::size_t j = 0U; j < selected_count; ++j) {
        const int dx = candidates_[i].x - selected_[j].x;
        const int dy = candidates_[i].y - selected_[j].y;
        if (dx * dx + dy * dy < 36) {
          spatially_separated = false;
          break;
        }
      }
      if (spatially_separated) {
        selected_[selected_count] = candidates_[i];
        minimum_selected_eigenvalue = candidates_[i].minimum_eigenvalue;
        ++selected_count;
      }
    }
    return selected_count;
  }

  // source patch를 target의 +/-4 px 정수 window에서 exhaustive하게 찾는다.
  // search가 9x9, patch가 5x5이므로 feature당 최대 2,025 pixel 차이를 계산한다.
  bool find_best_match(
    const std::array<std::uint8_t, kPixels> & source, const int source_x, const int source_y,
    const std::array<std::uint8_t, kPixels> & target, const int guess_x, const int guess_y,
    int & best_x, int & best_y, int & best_sad)
  {
    best_sad = std::numeric_limits<int>::max();
    bool found = false;
    for (int dy = -kSearchRadius; dy <= kSearchRadius; ++dy) {
      for (int dx = -kSearchRadius; dx <= kSearchRadius; ++dx) {
        const int candidate_x = guess_x + dx;
        const int candidate_y = guess_y + dy;
        if (!patch_is_inside(candidate_x, candidate_y)) {
          continue;
        }
        const int sad = patch_sad(source, source_x, source_y, target, candidate_x, candidate_y);
        if (sad < best_sad) {
          best_sad = sad;
          best_x = candidate_x;
          best_y = candidate_y;
          found = true;
        }
      }
    }
    return found;
  }

  // forward match 뒤 current→previous 역추적이 원점으로 돌아오는지 검사해 correspondence를 감시한다.
  std::size_t track_selected(
    const std::size_t selected_count, std::uint32_t & forward_backward_rejections,
    std::uint32_t & residual_rejections)
  {
    std::size_t tracked_count = 0U;
    for (std::size_t i = 0U; i < selected_count; ++i) {
      const auto & feature = selected_[i];
      int current_x = 0;
      int current_y = 0;
      int forward_sad = 0;
      if (!find_best_match(
          previous_, feature.x, feature.y, current_, feature.x, feature.y,
          current_x, current_y, forward_sad))
      {
        ++residual_rejections;
        continue;
      }

      // 5x5=25 pixel의 평균 절대 밝기 차이가 18을 넘으면 illumination/model mismatch로 버린다.
      const double mean_residual = static_cast<double>(forward_sad) / 25.0;
      if (mean_residual > 18.0) {
        ++residual_rejections;
        continue;
      }

      int backward_x = 0;
      int backward_y = 0;
      int backward_sad = 0;
      if (!find_best_match(
          current_, current_x, current_y, previous_, current_x, current_y,
          backward_x, backward_y, backward_sad))
      {
        ++forward_backward_rejections;
        continue;
      }
      const int round_trip_x = backward_x - feature.x;
      const int round_trip_y = backward_y - feature.y;
      if (round_trip_x * round_trip_x + round_trip_y * round_trip_y > 1) {
        ++forward_backward_rejections;
        continue;
      }

      tracks_[tracked_count] = Track{
        feature.x, feature.y, current_x, current_y, mean_residual};
      ++tracked_count;
    }
    return tracked_count;
  }

  // fixed array prefix의 median을 구한다. 복사와 sort 대상은 최대 48개다.
  double median_prefix(std::array<double, kMaxFeatures> & values, const std::size_t count)
  {
    if (count == 0U) {
      return 0.0;
    }
    std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(count));
    const std::size_t middle = count / 2U;
    if ((count % 2U) == 0U) {
      return 0.5 * (values[middle - 1U] + values[middle]);
    }
    return values[middle];
  }

  // 한 image를 copy→feature selection→bounded matching→metric translation 순으로 처리한다.
  void process_one_frame()
  {
    ImageConstPtr image;
    if (!frame_queue_.pop(image)) {
      return;
    }

    const bool valid_image = image->width == kWidth && image->height == kHeight &&
      image->encoding == "mono8" && image->step == kWidth && image->data.size() == kPixels &&
      image->header.frame_id == "camera_optical_frame";
    const bool valid_camera = camera_model_valid_.load(std::memory_order_acquire);
    if (!valid_image || !valid_camera) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000,
        "image or CameraInfo contract invalid; frame skipped");
      return;
    }

    std::copy_n(image->data.begin(), kPixels, current_.begin());

    daily_robotics_2026_10_25::msg::FeatureTrackStats stats;
    stats.header = image->header;
    stats.sequence = sequence_;
    stats.frame_queue_drops = frame_queue_drops_.load(std::memory_order_relaxed);
    stats.camera_model_valid = valid_camera;

    const auto kernel_start = std::chrono::steady_clock::now();
    if (have_previous_) {
      double minimum_eigenvalue = 0.0;
      const std::size_t selected_count = select_features(
        previous_, stats.candidate_count, minimum_eigenvalue);
      stats.selected_count = static_cast<std::uint32_t>(selected_count);
      stats.minimum_selected_eigenvalue = minimum_eigenvalue;

      const std::size_t tracked_count = track_selected(
        selected_count, stats.forward_backward_rejections, stats.residual_rejections);
      stats.tracked_count = static_cast<std::uint32_t>(tracked_count);

      for (std::size_t i = 0U; i < tracked_count; ++i) {
        flow_values_[i] = static_cast<double>(tracks_[i].current_x - tracks_[i].previous_x);
        residual_values_[i] = tracks_[i].residual;
      }
      stats.median_flow_x = median_prefix(flow_values_, tracked_count);
      stats.median_residual = median_prefix(residual_values_, tracked_count);

      if (tracked_count >= 12U) {
        // pinhole u=fx*X/Z를 frame 간 미분하면 dx_camera = -du*Z/fx다.
        // monocular 일반 장면에서는 Z/scale을 알 수 없지만 오늘은 4 m plane이라는 교육용 prior를 쓴다.
        const double focal = focal_length_px_.load(std::memory_order_relaxed);
        stats.estimated_delta_x = -stats.median_flow_x * kKnownPlaneDepthM / focal;
        accumulated_x_ += stats.estimated_delta_x;
      }
    }
    const auto kernel_end = std::chrono::steady_clock::now();

    stats.accumulated_x = accumulated_x_;
    stats.latest_kernel_us = std::chrono::duration<double, std::micro>(
      kernel_end - kernel_start).count();
    max_kernel_us_ = std::max(max_kernel_us_, stats.latest_kernel_us);
    stats.max_kernel_us = max_kernel_us_;
    stats.bounds_respected = stats.candidate_count <= kMaxCandidates &&
      stats.selected_count <= kMaxFeatures && stats.tracked_count <= kMaxFeatures;

    nav_msgs::msg::Odometry odometry;
    odometry.header = image->header;
    odometry.header.frame_id = "map";
    odometry.child_frame_id = "camera_link";
    odometry.pose.pose.position.x = accumulated_x_;
    odometry.pose.pose.orientation.w = 1.0;
    // 추적 feature가 충분한 합성 장면의 교육용 covariance다. 실제 시스템은 residual/geometry로 추정해야 한다.
    odometry.pose.covariance[0] = (stats.tracked_count >= 12U) ? 0.0004 : 1000.0;
    odometry.pose.covariance[7] = 1000.0;
    odometry.pose.covariance[14] = 1000.0;
    odometry.pose.covariance[21] = 1000.0;
    odometry.pose.covariance[28] = 1000.0;
    odometry.pose.covariance[35] = 1000.0;

    stats_publisher_->publish(stats);
    odometry_publisher_->publish(odometry);

    // double buffer 대신 fixed arrays를 swap해 current가 다음 frame의 previous가 되게 한다.
    previous_.swap(current_);
    have_previous_ = true;
    ++sequence_;
  }

  rclcpp::CallbackGroup::SharedPtr input_group_;
  rclcpp::CallbackGroup::SharedPtr worker_group_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr info_subscription_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odometry_publisher_;
  rclcpp::Publisher<daily_robotics_2026_10_25::msg::FeatureTrackStats>::SharedPtr stats_publisher_;
  rclcpp::TimerBase::SharedPtr worker_timer_;

  SpscRing<ImageConstPtr, kFrameQueueSlots> frame_queue_;
  std::atomic<std::uint32_t> frame_queue_drops_{0U};
  std::atomic<double> focal_length_px_{0.0};
  std::atomic<bool> camera_model_valid_{false};

  std::array<std::uint8_t, kPixels> previous_{};
  std::array<std::uint8_t, kPixels> current_{};
  std::array<Candidate, kMaxCandidates> candidates_{};
  std::array<Candidate, kMaxFeatures> selected_{};
  std::array<Track, kMaxFeatures> tracks_{};
  std::array<double, kMaxFeatures> flow_values_{};
  std::array<double, kMaxFeatures> residual_values_{};
  bool have_previous_{false};
  std::uint32_t sequence_{0U};
  double accumulated_x_{0.0};
  double max_kernel_us_{0.0};
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<BoundedFeatureTracker>();

  // 두 callback group이 실제로 병렬 dispatch될 수 있도록 worker 두 개인 executor를 사용한다.
  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions(), 2U);
  executor.add_node(node);
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
