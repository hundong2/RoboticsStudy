#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>

#include "daily_robotics_2026_11_10/msg/ndt_stats.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/point_field.hpp"

namespace
{

constexpr std::size_t kMapCapacity = 192U;
constexpr std::size_t kScanCapacity = 96U;
constexpr std::size_t kGridWidth = 20U;
constexpr std::size_t kGridCellCount = kGridWidth * kGridWidth;
constexpr std::size_t kMaximumIterations = 6U;
constexpr double kGridMinimum = -4.0;
constexpr double kCellSize = 0.4;
constexpr double kCovarianceRegularization = 0.0025;
constexpr double kMahalanobisGate = 25.0;
constexpr double kPi = 3.14159265358979323846;

struct Point2
{
  double x{};
  double y{};
};

struct Pose2
{
  double x{};
  double y{};
  double yaw{};
};

struct Cell
{
  std::uint32_t count{};
  double sum_x{};
  double sum_y{};
  double sum_xx{};
  double sum_xy{};
  double sum_yy{};
  double mean_x{};
  double mean_y{};
  double inverse_xx{};
  double inverse_xy{};
  double inverse_yy{};
  bool valid{};
};

double normalize_angle(double angle)
{
  while (angle > kPi) {
    angle -= 2.0 * kPi;
  }
  while (angle < -kPi) {
    angle += 2.0 * kPi;
  }
  return angle;
}

std::int64_t stamp_to_nanoseconds(const builtin_interfaces::msg::Time & stamp)
{
  return static_cast<std::int64_t>(stamp.sec) * 1'000'000'000LL +
         static_cast<std::int64_t>(stamp.nanosec);
}

}  // namespace

/**
 * reference cloud를 20x20 Gaussian cell로 바꾸고 current scan의 SE(2) pose를 최대 6회
 * Gauss-Newton으로 추정한다. callback의 점/셀/반복 수가 모두 compile-time 상한을 가진다.
 */
class BoundedNdtMatcher : public rclcpp::Node
{
public:
  BoundedNdtMatcher()
  : Node("bounded_ndt_matcher")
  {
    stats_publisher_ = create_publisher<daily_robotics_2026_11_10::msg::NdtStats>(
      "/ndt/stats", rclcpp::QoS(10).reliable());

    // reference publisher와 durability/reliability가 맞아야 late-joiner가 보관된 map을 받는다.
    reference_subscription_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      "/ndt/reference", rclcpp::QoS(1).reliable().transient_local(),
      std::bind(&BoundedNdtMatcher::on_reference, this, std::placeholders::_1));

    // 센서 stream의 offered QoS와 맞추기 위해 SensorDataQoS를 사용한다.
    scan_subscription_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      "/ndt/scan", rclcpp::SensorDataQoS().keep_last(1),
      std::bind(&BoundedNdtMatcher::on_scan, this, std::placeholders::_1));
  }

private:
  // PointCloud2 schema를 신뢰하지 않고 x/y FLOAT32 offset과 buffer 경계를 검사한다.
  template<std::size_t Capacity>
  std::size_t decode_xy(
    const sensor_msgs::msg::PointCloud2 & cloud,
    std::array<Point2, Capacity> & output,
    std::uint32_t & rejected) const
  {
    rejected = 0U;
    if (cloud.is_bigendian || cloud.point_step < 8U || cloud.height == 0U) {
      return 0U;
    }

    std::uint32_t x_offset = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t y_offset = std::numeric_limits<std::uint32_t>::max();
    for (const auto & field : cloud.fields) {
      const bool scalar_float =
        field.datatype == sensor_msgs::msg::PointField::FLOAT32 && field.count == 1U;
      if (field.name == "x" && scalar_float) {
        x_offset = field.offset;
      } else if (field.name == "y" && scalar_float) {
        y_offset = field.offset;
      }
    }
    if (x_offset == std::numeric_limits<std::uint32_t>::max() ||
      y_offset == std::numeric_limits<std::uint32_t>::max() ||
      x_offset + sizeof(float) > cloud.point_step ||
      y_offset + sizeof(float) > cloud.point_step)
    {
      return 0U;
    }

    const std::uint64_t declared_count =
      static_cast<std::uint64_t>(cloud.width) * static_cast<std::uint64_t>(cloud.height);
    const std::uint64_t buffer_count = cloud.data.size() / cloud.point_step;
    const std::size_t available = static_cast<std::size_t>(std::min(declared_count, buffer_count));
    const std::size_t inspected = std::min(available, Capacity);
    rejected += static_cast<std::uint32_t>(available - inspected);

    std::size_t accepted = 0U;
    for (std::size_t i = 0U; i < inspected; ++i) {
      const std::size_t base = i * static_cast<std::size_t>(cloud.point_step);
      float x = 0.0F;
      float y = 0.0F;
      std::memcpy(&x, cloud.data.data() + base + x_offset, sizeof(float));
      std::memcpy(&y, cloud.data.data() + base + y_offset, sizeof(float));
      if (!std::isfinite(x) || !std::isfinite(y)) {
        ++rejected;
        continue;
      }
      output[accepted++] = Point2{static_cast<double>(x), static_cast<double>(y)};
    }
    return accepted;
  }

  bool cell_index(double x, double y, std::size_t & index) const
  {
    const int column = static_cast<int>(std::floor((x - kGridMinimum) / kCellSize));
    const int row = static_cast<int>(std::floor((y - kGridMinimum) / kCellSize));
    if (column < 0 || row < 0 || column >= static_cast<int>(kGridWidth) ||
      row >= static_cast<int>(kGridWidth))
    {
      return false;
    }
    index = static_cast<std::size_t>(row) * kGridWidth + static_cast<std::size_t>(column);
    return true;
  }

  // 각 셀에서 평균 mu와 공분산 Sigma를 만들고, hot path가 바로 쓸 Sigma^{-1}을 저장한다.
  void build_ndt_grid(const std::array<Point2, kMapCapacity> & points, std::size_t count)
  {
    cells_.fill(Cell{});
    occupied_cells_ = 0U;
    for (std::size_t i = 0U; i < count; ++i) {
      std::size_t index = 0U;
      if (!cell_index(points[i].x, points[i].y, index)) {
        continue;
      }
      Cell & cell = cells_[index];
      ++cell.count;
      cell.sum_x += points[i].x;
      cell.sum_y += points[i].y;
      cell.sum_xx += points[i].x * points[i].x;
      cell.sum_xy += points[i].x * points[i].y;
      cell.sum_yy += points[i].y * points[i].y;
    }

    for (Cell & cell : cells_) {
      if (cell.count < 4U) {
        continue;
      }
      const double n = static_cast<double>(cell.count);
      cell.mean_x = cell.sum_x / n;
      cell.mean_y = cell.sum_y / n;
      const double denominator = n - 1.0;
      // Sigma = E[(p-mu)(p-mu)^T]. 작은 lambda I는 선 모양 셀의 특이행렬을 막는다.
      const double covariance_xx =
        (cell.sum_xx - n * cell.mean_x * cell.mean_x) / denominator +
        kCovarianceRegularization;
      const double covariance_xy =
        (cell.sum_xy - n * cell.mean_x * cell.mean_y) / denominator;
      const double covariance_yy =
        (cell.sum_yy - n * cell.mean_y * cell.mean_y) / denominator +
        kCovarianceRegularization;
      const double determinant =
        covariance_xx * covariance_yy - covariance_xy * covariance_xy;
      if (determinant <= 1.0e-12) {
        continue;
      }
      cell.inverse_xx = covariance_yy / determinant;
      cell.inverse_xy = -covariance_xy / determinant;
      cell.inverse_yy = covariance_xx / determinant;
      cell.valid = true;
      ++occupied_cells_;
    }
  }

  void on_reference(const sensor_msgs::msg::PointCloud2::SharedPtr message)
  {
    std::uint32_t rejected = 0U;
    const std::size_t count = decode_xy(*message, map_points_, rejected);
    if (message->header.frame_id != "map" || count < 80U) {
      RCLCPP_ERROR(get_logger(), "Rejected reference cloud: frame=%s count=%zu", message->header.frame_id.c_str(), count);
      return;
    }
    build_ndt_grid(map_points_, count);
    reference_ready_ = occupied_cells_ >= 12U;
    RCLCPP_INFO(
      get_logger(), "NDT grid ready=%s points=%zu cells=%u rejected=%u",
      reference_ready_ ? "true" : "false", count, occupied_cells_, rejected);
  }

  // 3x3 partial-pivot Gaussian elimination으로 H delta = -g를 푼다.
  bool solve_three_by_three(
    const std::array<double, 9U> & hessian,
    const std::array<double, 3U> & gradient,
    std::array<double, 3U> & delta,
    double & minimum_pivot) const
  {
    std::array<std::array<double, 4U>, 3U> augmented{};
    for (std::size_t row = 0U; row < 3U; ++row) {
      for (std::size_t column = 0U; column < 3U; ++column) {
        augmented[row][column] = hessian[row * 3U + column];
      }
      augmented[row][3U] = -gradient[row];
    }

    minimum_pivot = std::numeric_limits<double>::max();
    for (std::size_t pivot_column = 0U; pivot_column < 3U; ++pivot_column) {
      std::size_t pivot_row = pivot_column;
      for (std::size_t row = pivot_column + 1U; row < 3U; ++row) {
        if (std::abs(augmented[row][pivot_column]) >
          std::abs(augmented[pivot_row][pivot_column]))
        {
          pivot_row = row;
        }
      }
      if (pivot_row != pivot_column) {
        std::swap(augmented[pivot_row], augmented[pivot_column]);
      }
      const double pivot = std::abs(augmented[pivot_column][pivot_column]);
      minimum_pivot = std::min(minimum_pivot, pivot);
      if (pivot < 1.0e-8) {
        return false;
      }
      for (std::size_t row = pivot_column + 1U; row < 3U; ++row) {
        const double factor =
          augmented[row][pivot_column] / augmented[pivot_column][pivot_column];
        for (std::size_t column = pivot_column; column < 4U; ++column) {
          augmented[row][column] -= factor * augmented[pivot_column][column];
        }
      }
    }

    for (int row = 2; row >= 0; --row) {
      double rhs = augmented[static_cast<std::size_t>(row)][3U];
      for (std::size_t column = static_cast<std::size_t>(row) + 1U; column < 3U; ++column) {
        rhs -= augmented[static_cast<std::size_t>(row)][column] * delta[column];
      }
      delta[static_cast<std::size_t>(row)] =
        rhs / augmented[static_cast<std::size_t>(row)][static_cast<std::size_t>(row)];
    }
    return true;
  }

  void on_scan(const sensor_msgs::msg::PointCloud2::SharedPtr message)
  {
    if (!reference_ready_ || message->header.frame_id != "lidar") {
      return;
    }
    const auto kernel_start = std::chrono::steady_clock::now();
    std::uint32_t rejected = 0U;
    const std::size_t scan_count = decode_xy(*message, scan_points_, rejected);
    if (scan_count < 40U) {
      return;
    }

    Pose2 estimate = previous_estimate_;  // 이전 결과는 연속 센서 stream의 bounded 초기값이다.
    std::uint32_t solve_failures = 0U;
    std::uint32_t used_points = 0U;
    std::size_t completed_iterations = 0U;
    double minimum_pivot = std::numeric_limits<double>::max();
    double mean_cost = std::numeric_limits<double>::infinity();
    bool converged = false;

    for (std::size_t iteration = 0U; iteration < kMaximumIterations; ++iteration) {
      std::array<double, 9U> hessian{};
      std::array<double, 3U> gradient{};
      double total_cost = 0.0;
      used_points = 0U;
      const double c = std::cos(estimate.yaw);
      const double s = std::sin(estimate.yaw);

      for (std::size_t i = 0U; i < scan_count; ++i) {
        // p_m = R(theta) p_l + t. yaw에 대한 미분은 [-s*x-c*y, c*x-s*y]^T다.
        const double transformed_x = c * scan_points_[i].x - s * scan_points_[i].y + estimate.x;
        const double transformed_y = s * scan_points_[i].x + c * scan_points_[i].y + estimate.y;
        std::size_t index = 0U;
        if (!cell_index(transformed_x, transformed_y, index) || !cells_[index].valid) {
          continue;
        }
        const Cell & cell = cells_[index];
        const double residual_x = transformed_x - cell.mean_x;
        const double residual_y = transformed_y - cell.mean_y;
        const double weighted_x =
          cell.inverse_xx * residual_x + cell.inverse_xy * residual_y;
        const double weighted_y =
          cell.inverse_xy * residual_x + cell.inverse_yy * residual_y;
        const double mahalanobis = residual_x * weighted_x + residual_y * weighted_y;
        if (mahalanobis > kMahalanobisGate) {
          continue;
        }

        const std::array<double, 6U> jacobian{
          1.0, 0.0, -s * scan_points_[i].x - c * scan_points_[i].y,
          0.0, 1.0, c * scan_points_[i].x - s * scan_points_[i].y};
        for (std::size_t row = 0U; row < 3U; ++row) {
          const double weighted_residual =
            jacobian[row] * weighted_x + jacobian[3U + row] * weighted_y;
          gradient[row] += weighted_residual;
          for (std::size_t column = 0U; column < 3U; ++column) {
            hessian[row * 3U + column] +=
              jacobian[row] *
              (cell.inverse_xx * jacobian[column] +
              cell.inverse_xy * jacobian[3U + column]) +
              jacobian[3U + row] *
              (cell.inverse_xy * jacobian[column] +
              cell.inverse_yy * jacobian[3U + column]);
          }
        }
        total_cost += mahalanobis;
        ++used_points;
      }

      if (used_points < 30U) {
        ++solve_failures;
        break;
      }
      // 작은 Levenberg 대각항은 수치적으로 약한 방향의 step 폭주를 완화한다.
      hessian[0U] += 1.0e-3;
      hessian[4U] += 1.0e-3;
      hessian[8U] += 1.0e-3;

      std::array<double, 3U> delta{};
      double iteration_pivot = 0.0;
      if (!solve_three_by_three(hessian, gradient, delta, iteration_pivot)) {
        ++solve_failures;
        break;
      }
      minimum_pivot = std::min(minimum_pivot, iteration_pivot);
      // 잘못된 cell association이 한 번에 큰 자세 jump를 만들지 못하도록 trust region을 둔다.
      delta[0U] = std::clamp(delta[0U], -0.08, 0.08);
      delta[1U] = std::clamp(delta[1U], -0.08, 0.08);
      delta[2U] = std::clamp(delta[2U], -0.04, 0.04);
      estimate.x += delta[0U];
      estimate.y += delta[1U];
      estimate.yaw = normalize_angle(estimate.yaw + delta[2U]);
      mean_cost = total_cost / static_cast<double>(used_points);
      completed_iterations = iteration + 1U;
      const double step_norm =
        std::sqrt(delta[0U] * delta[0U] + delta[1U] * delta[1U] + delta[2U] * delta[2U]);
      if (step_norm < 1.0e-4) {
        converged = true;
        break;
      }
    }

    previous_estimate_ = estimate;
    const auto elapsed = std::chrono::steady_clock::now() - kernel_start;
    const double kernel_time_us =
      std::chrono::duration<double, std::micro>(elapsed).count();

    daily_robotics_2026_11_10::msg::NdtStats stats;
    stats.header = message->header;  // acquisition stamp를 보존해 truth와 exact-time 감사한다.
    stats.sequence = sequence_++;
    stats.estimated_x = estimate.x;
    stats.estimated_y = estimate.y;
    stats.estimated_yaw = estimate.yaw;
    stats.mean_mahalanobis_cost = mean_cost;
    stats.valid_points = used_points;
    stats.occupied_cells = occupied_cells_;
    stats.iterations = static_cast<std::uint32_t>(completed_iterations);
    stats.rejected_points = rejected;
    stats.solve_failures = solve_failures;
    stats.minimum_hessian_pivot =
      std::isfinite(minimum_pivot) ? minimum_pivot : 0.0;
    stats.kernel_time_us = kernel_time_us;
    stats.converged = converged || (solve_failures == 0U && used_points >= 30U);
    stats_publisher_->publish(stats);

    if (stats.sequence % 20U == 0U) {
      RCLCPP_INFO(
        get_logger(), "NDT seq=%u pose=(%.3f, %.3f, %.3f) used=%u iter=%u cost=%.3f us=%.1f stamp=%lld",
        stats.sequence, estimate.x, estimate.y, estimate.yaw, stats.valid_points,
        stats.iterations, stats.mean_mahalanobis_cost, stats.kernel_time_us,
        static_cast<long long>(stamp_to_nanoseconds(stats.header.stamp)));
    }
  }

  std::array<Point2, kMapCapacity> map_points_{};
  std::array<Point2, kScanCapacity> scan_points_{};
  std::array<Cell, kGridCellCount> cells_{};
  Pose2 previous_estimate_{};
  std::uint32_t occupied_cells_{0U};
  std::uint32_t sequence_{0U};
  bool reference_ready_{false};
  rclcpp::Publisher<daily_robotics_2026_11_10::msg::NdtStats>::SharedPtr stats_publisher_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr reference_subscription_;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr scan_subscription_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BoundedNdtMatcher>());
  rclcpp::shutdown();
  return 0;
}
