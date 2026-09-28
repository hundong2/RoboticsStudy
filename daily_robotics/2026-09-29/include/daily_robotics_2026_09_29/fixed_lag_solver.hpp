#ifndef DAILY_ROBOTICS_2026_09_29__FIXED_LAG_SOLVER_HPP_
#define DAILY_ROBOTICS_2026_09_29__FIXED_LAG_SOLVER_HPP_

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace daily_robotics_2026_09_29
{

// Vec2는 평면 위치 [x, y]를 나타낸다. std::array는 크기가 컴파일 때 정해져
// hot path에서 힙 할당이 일어나지 않는 C++17 고정 길이 컨테이너다.
using Vec2 = std::array<double, 2>;

class FixedLagSolver
{
public:
  static constexpr std::size_t kWindow = 8U;
  static constexpr std::size_t kMaxDim = 2U * kWindow;
  static constexpr std::size_t kIrlsIterations = 4U;

  struct State
  {
    std::int64_t stamp_ns{0};
    Vec2 estimate{{0.0, 0.0}};
    Vec2 raw_odom{{0.0, 0.0}};
    bool has_gnss{false};
    Vec2 gnss{{0.0, 0.0}};
    double gnss_sigma_m{0.4};
    bool downweight_counted{false};
  };

  struct Report
  {
    bool solved{false};
    double minimum_gnss_weight{1.0};
    double latest_variance_x{0.0};
    double latest_variance_y{0.0};
    double pivot_condition_proxy{1.0};
    double solve_time_us{0.0};
  };

  // 새 wheel odometry pose를 sliding window에 넣는다. 창이 가득 찼으면 가장 오래된
  // pose를 먼저 Schur complement로 제거하므로 상태 차원은 항상 16 이하이다.
  void add_odometry(const std::int64_t stamp_ns, const Vec2 & raw_odom)
  {
    if (size_ == kWindow) {
      marginalize_oldest();
    }

    State next;
    next.stamp_ns = stamp_ns;
    next.raw_odom = raw_odom;
    if (size_ == 0U) {
      next.estimate = raw_odom;
      prior_mean_ = raw_odom;
      // 첫 pose를 10 cm 표준편차로 고정해 global translation gauge freedom을 없앤다.
      prior_information_ = {{100.0, 0.0, 0.0, 100.0}};
    } else {
      const State & previous = states_[size_ - 1U];
      // odometry의 절대 pose를 믿는 대신 연속 pose 차이 Δz를 motion factor로 사용한다.
      next.estimate[0] = previous.estimate[0] + raw_odom[0] - previous.raw_odom[0];
      next.estimate[1] = previous.estimate[1] + raw_odom[1] - previous.raw_odom[1];
    }
    states_[size_] = next;
    ++size_;
  }

  // GNSS를 Header.stamp가 가장 가까운 window state에 연결한다. arrival time이 아니라
  // 측정 시각을 사용하며, 60 ms를 넘는 pair는 잘못된 factor가 되지 않도록 거부한다.
  bool attach_gnss(
    const std::int64_t stamp_ns, const Vec2 & position, const double sigma_m,
    const std::int64_t maximum_skew_ns)
  {
    if (size_ == 0U) {
      ++gnss_rejected_;
      return false;
    }

    std::size_t best = 0U;
    std::int64_t best_skew = std::numeric_limits<std::int64_t>::max();
    for (std::size_t i = 0U; i < size_; ++i) {
      const std::int64_t difference = states_[i].stamp_ns - stamp_ns;
      const std::int64_t skew = difference >= 0 ? difference : -difference;
      if (skew < best_skew) {
        best_skew = skew;
        best = i;
      }
    }

    if (best_skew > maximum_skew_ns) {
      ++gnss_rejected_;
      return false;
    }

    states_[best].has_gnss = true;
    states_[best].gnss = position;
    states_[best].gnss_sigma_m = std::max(0.05, sigma_m);
    states_[best].downweight_counted = false;
    ++gnss_associated_;
    maximum_pair_skew_ms_ = std::max(
      maximum_pair_skew_ms_, static_cast<double>(best_skew) * 1.0e-6);
    return true;
  }

  // 4회 고정 IRLS로 Huber weight와 선형 최소제곱 해를 번갈아 갱신한다.
  // 데이터 개수와 반복 수가 모두 상한을 가지므로 계산량이 입력에 따라 무한히 늘지 않는다.
  Report optimize()
  {
    Report report;
    if (size_ == 0U) {
      return report;
    }

    const auto begin = std::chrono::steady_clock::now();
    double condition_proxy = 1.0;
    for (std::size_t iteration = 0U; iteration < kIrlsIterations; ++iteration) {
      double minimum_weight = 1.0;
      build_system(minimum_weight);
      std::array<double, kMaxDim> increment{};
      double iteration_condition = 1.0;
      const std::size_t dimension = 2U * size_;
      if (!solve_linear(hessian_, gradient_, dimension, increment, iteration_condition)) {
        return report;
      }
      condition_proxy = std::max(condition_proxy, iteration_condition);

      for (std::size_t i = 0U; i < size_; ++i) {
        double dx = increment[2U * i];
        double dy = increment[2U * i + 1U];
        const double norm = std::hypot(dx, dy);
        // 잘못된 초기값이 들어와도 한 번에 2 m 이상 뛰지 않게 trust-region 역할을 한다.
        if (norm > 2.0) {
          const double scale = 2.0 / norm;
          dx *= scale;
          dy *= scale;
        }
        states_[i].estimate[0] += dx;
        states_[i].estimate[1] += dy;
      }
    }

    // 마지막 선형화점에서 품질 통계와 covariance를 계산한다.
    build_system(report.minimum_gnss_weight);
    const std::size_t dimension = 2U * size_;
    const std::size_t x_index = dimension - 2U;
    const std::size_t y_index = dimension - 1U;
    std::array<double, kMaxDim> unit_rhs{};
    std::array<double, kMaxDim> inverse_column{};
    unit_rhs[x_index] = 1.0;
    double covariance_condition = 1.0;
    if (solve_linear(hessian_, unit_rhs, dimension, inverse_column, covariance_condition)) {
      report.latest_variance_x = std::max(0.0, inverse_column[x_index]);
    }
    unit_rhs.fill(0.0);
    inverse_column.fill(0.0);
    unit_rhs[y_index] = 1.0;
    if (solve_linear(hessian_, unit_rhs, dimension, inverse_column, covariance_condition)) {
      report.latest_variance_y = std::max(0.0, inverse_column[y_index]);
    }

    // 동일 outlier가 네 IRLS 반복마다 중복 집계되지 않도록 state별 latch를 둔다.
    for (std::size_t i = 0U; i < size_; ++i) {
      if (!states_[i].has_gnss) {
        continue;
      }
      const double weight = gnss_weight(states_[i]);
      if (weight < 0.5 && !states_[i].downweight_counted) {
        states_[i].downweight_counted = true;
        ++downweighted_outliers_;
      }
    }

    const auto end = std::chrono::steady_clock::now();
    report.solve_time_us =
      std::chrono::duration<double, std::micro>(end - begin).count();
    maximum_solve_time_us_ = std::max(maximum_solve_time_us_, report.solve_time_us);
    report.pivot_condition_proxy = std::max(condition_proxy, covariance_condition);
    report.solved = std::isfinite(report.latest_variance_x) &&
      std::isfinite(report.latest_variance_y);
    return report;
  }

  const State & newest() const {return states_[size_ - 1U];}
  std::size_t size() const {return size_;}
  std::uint32_t marginalizations() const {return marginalizations_;}
  std::uint32_t gnss_associated() const {return gnss_associated_;}
  std::uint32_t gnss_rejected() const {return gnss_rejected_;}
  std::uint32_t downweighted_outliers() const {return downweighted_outliers_;}
  double maximum_pair_skew_ms() const {return maximum_pair_skew_ms_;}
  double maximum_solve_time_us() const {return maximum_solve_time_us_;}

private:
  using Matrix = std::array<double, kMaxDim * kMaxDim>;
  using Vector = std::array<double, kMaxDim>;

  static constexpr double kOdomSigmaM = 0.06;
  static constexpr double kHuberThreshold = 2.0;

  static std::size_t index(const std::size_t row, const std::size_t column)
  {
    return row * kMaxDim + column;
  }

  double gnss_weight(const State & state) const
  {
    const double rx = state.estimate[0] - state.gnss[0];
    const double ry = state.estimate[1] - state.gnss[1];
    // normalized residual s = ||r||/σ. Huber는 s<=δ이면 1, 그 밖은 δ/s다.
    const double normalized = std::hypot(rx, ry) / state.gnss_sigma_m;
    return normalized <= kHuberThreshold ? 1.0 : kHuberThreshold / normalized;
  }

  void build_system(double & minimum_gnss_weight)
  {
    hessian_.fill(0.0);
    gradient_.fill(0.0);
    minimum_gnss_weight = 1.0;

    // Prior factor: 0.5 * (x0-mu)^T P (x0-mu).
    const Vec2 prior_residual{{
      states_[0].estimate[0] - prior_mean_[0],
      states_[0].estimate[1] - prior_mean_[1]}};
    for (std::size_t row = 0U; row < 2U; ++row) {
      for (std::size_t column = 0U; column < 2U; ++column) {
        hessian_[index(row, column)] += prior_information_[2U * row + column];
        gradient_[row] -= prior_information_[2U * row + column] * prior_residual[column];
      }
    }

    // Odometry factor: r_i = (x_i-x_{i-1}) - (z_i-z_{i-1}).
    const double odom_information = 1.0 / (kOdomSigmaM * kOdomSigmaM);
    for (std::size_t i = 1U; i < size_; ++i) {
      for (std::size_t axis = 0U; axis < 2U; ++axis) {
        const std::size_t previous = 2U * (i - 1U) + axis;
        const std::size_t current = 2U * i + axis;
        const double measured_delta = states_[i].raw_odom[axis] - states_[i - 1U].raw_odom[axis];
        const double residual =
          states_[i].estimate[axis] - states_[i - 1U].estimate[axis] - measured_delta;
        hessian_[index(previous, previous)] += odom_information;
        hessian_[index(current, current)] += odom_information;
        hessian_[index(previous, current)] -= odom_information;
        hessian_[index(current, previous)] -= odom_information;
        gradient_[previous] += odom_information * residual;
        gradient_[current] -= odom_information * residual;
      }
    }

    // GNSS absolute factor에 Huber IRLS weight를 곱혀 큰 이상치의 영향력을 제한한다.
    for (std::size_t i = 0U; i < size_; ++i) {
      if (!states_[i].has_gnss) {
        continue;
      }
      const double robust_weight = gnss_weight(states_[i]);
      minimum_gnss_weight = std::min(minimum_gnss_weight, robust_weight);
      const double information =
        robust_weight / (states_[i].gnss_sigma_m * states_[i].gnss_sigma_m);
      for (std::size_t axis = 0U; axis < 2U; ++axis) {
        const std::size_t variable = 2U * i + axis;
        const double residual = states_[i].estimate[axis] - states_[i].gnss[axis];
        hessian_[index(variable, variable)] += information;
        gradient_[variable] -= information * residual;
      }
    }

    // 1e-9 diagonal은 floating-point pivot가 정확히 0이 되는 것을 막는 수치 감쇠다.
    for (std::size_t i = 0U; i < 2U * size_; ++i) {
      hessian_[index(i, i)] += 1.0e-9;
    }
  }

  // 부분 pivot Gaussian elimination. 최대 16x16, 반복 상한이 명확하며 heap을 쓰지 않는다.
  static bool solve_linear(
    const Matrix & matrix, const Vector & right_hand_side, const std::size_t dimension,
    Vector & solution, double & condition_proxy)
  {
    std::array<double, kMaxDim * (kMaxDim + 1U)> augmented{};
    constexpr std::size_t stride = kMaxDim + 1U;
    for (std::size_t row = 0U; row < dimension; ++row) {
      for (std::size_t column = 0U; column < dimension; ++column) {
        augmented[row * stride + column] = matrix[index(row, column)];
      }
      augmented[row * stride + dimension] = right_hand_side[row];
    }

    double minimum_pivot = std::numeric_limits<double>::max();
    double maximum_pivot = 0.0;
    for (std::size_t pivot_column = 0U; pivot_column < dimension; ++pivot_column) {
      std::size_t pivot_row = pivot_column;
      double pivot_magnitude = std::abs(augmented[pivot_row * stride + pivot_column]);
      for (std::size_t candidate = pivot_column + 1U; candidate < dimension; ++candidate) {
        const double magnitude = std::abs(augmented[candidate * stride + pivot_column]);
        if (magnitude > pivot_magnitude) {
          pivot_magnitude = magnitude;
          pivot_row = candidate;
        }
      }
      if (pivot_magnitude < 1.0e-12 || !std::isfinite(pivot_magnitude)) {
        return false;
      }
      if (pivot_row != pivot_column) {
        for (std::size_t column = pivot_column; column <= dimension; ++column) {
          std::swap(
            augmented[pivot_column * stride + column],
            augmented[pivot_row * stride + column]);
        }
      }

      const double pivot = augmented[pivot_column * stride + pivot_column];
      minimum_pivot = std::min(minimum_pivot, std::abs(pivot));
      maximum_pivot = std::max(maximum_pivot, std::abs(pivot));
      for (std::size_t column = pivot_column; column <= dimension; ++column) {
        augmented[pivot_column * stride + column] /= pivot;
      }
      for (std::size_t row = 0U; row < dimension; ++row) {
        if (row == pivot_column) {
          continue;
        }
        const double factor = augmented[row * stride + pivot_column];
        for (std::size_t column = pivot_column; column <= dimension; ++column) {
          augmented[row * stride + column] -=
            factor * augmented[pivot_column * stride + column];
        }
      }
    }

    solution.fill(0.0);
    for (std::size_t row = 0U; row < dimension; ++row) {
      solution[row] = augmented[row * stride + dimension];
      if (!std::isfinite(solution[row])) {
        return false;
      }
    }
    condition_proxy = maximum_pivot / std::max(minimum_pivot, 1.0e-12);
    return true;
  }

  static bool invert_2x2(const std::array<double, 4> & matrix, std::array<double, 4> & inverse)
  {
    const double determinant = matrix[0] * matrix[3] - matrix[1] * matrix[2];
    if (std::abs(determinant) < 1.0e-12 || !std::isfinite(determinant)) {
      return false;
    }
    inverse = {{
      matrix[3] / determinant, -matrix[1] / determinant,
      -matrix[2] / determinant, matrix[0] / determinant}};
    return true;
  }

  static Vec2 multiply_2x2(const std::array<double, 4> & matrix, const Vec2 & vector)
  {
    return {{
      matrix[0] * vector[0] + matrix[1] * vector[1],
      matrix[2] * vector[0] + matrix[3] * vector[1]}};
  }

  // 가장 오래된 x0를 제거하고 x1에 equivalent Gaussian prior를 남긴다.
  // Hm = Hrr - Hro * Hoo^-1 * Hor 는 Schur complement 공식이다.
  void marginalize_oldest()
  {
    if (size_ < 2U) {
      return;
    }

    const double odom_information = 1.0 / (kOdomSigmaM * kOdomSigmaM);
    std::array<double, 4> h_oo = prior_information_;
    h_oo[0] += odom_information;
    h_oo[3] += odom_information;
    Vec2 g_o{{0.0, 0.0}};
    const Vec2 prior_residual{{
      states_[0].estimate[0] - prior_mean_[0],
      states_[0].estimate[1] - prior_mean_[1]}};
    const Vec2 prior_term = multiply_2x2(prior_information_, prior_residual);
    g_o[0] -= prior_term[0];
    g_o[1] -= prior_term[1];

    Vec2 g_r{{0.0, 0.0}};
    for (std::size_t axis = 0U; axis < 2U; ++axis) {
      const double measured_delta = states_[1].raw_odom[axis] - states_[0].raw_odom[axis];
      const double residual =
        states_[1].estimate[axis] - states_[0].estimate[axis] - measured_delta;
      g_o[axis] += odom_information * residual;
      g_r[axis] -= odom_information * residual;
    }

    if (states_[0].has_gnss) {
      const double information = gnss_weight(states_[0]) /
        (states_[0].gnss_sigma_m * states_[0].gnss_sigma_m);
      h_oo[0] += information;
      h_oo[3] += information;
      g_o[0] -= information * (states_[0].estimate[0] - states_[0].gnss[0]);
      g_o[1] -= information * (states_[0].estimate[1] - states_[0].gnss[1]);
    }

    std::array<double, 4> inverse_h_oo{};
    bool valid = invert_2x2(h_oo, inverse_h_oo);
    std::array<double, 4> marginalized_information{};
    Vec2 marginalized_gradient{{0.0, 0.0}};
    if (valid) {
      // Hro=Hor=-wI 이므로 Hm=wI-w^2*Hoo^-1,
      // gm=gr+w*Hoo^-1*go 로 단순화된다.
      marginalized_information = {{
        odom_information - odom_information * odom_information * inverse_h_oo[0],
        -odom_information * odom_information * inverse_h_oo[1],
        -odom_information * odom_information * inverse_h_oo[2],
        odom_information - odom_information * odom_information * inverse_h_oo[3]}};
      const Vec2 corrected = multiply_2x2(inverse_h_oo, g_o);
      marginalized_gradient[0] = g_r[0] + odom_information * corrected[0];
      marginalized_gradient[1] = g_r[1] + odom_information * corrected[1];
    }

    std::array<double, 4> inverse_marginal{};
    valid = valid && invert_2x2(marginalized_information, inverse_marginal);
    if (valid) {
      const Vec2 mean_correction = multiply_2x2(inverse_marginal, marginalized_gradient);
      prior_mean_ = {{
        states_[1].estimate[0] + mean_correction[0],
        states_[1].estimate[1] + mean_correction[1]}};
      prior_information_ = marginalized_information;
    } else {
      // 수치 실패 시 오래된 정보를 과신하지 않고 현재 x1 주위의 완만한 prior로 fallback한다.
      prior_mean_ = states_[1].estimate;
      prior_information_ = {{25.0, 0.0, 0.0, 25.0}};
    }

    for (std::size_t i = 1U; i < size_; ++i) {
      states_[i - 1U] = states_[i];
    }
    --size_;
    ++marginalizations_;
  }

  std::array<State, kWindow> states_{};
  std::size_t size_{0U};
  Vec2 prior_mean_{{0.0, 0.0}};
  std::array<double, 4> prior_information_{{100.0, 0.0, 0.0, 100.0}};
  Matrix hessian_{};
  Vector gradient_{};
  std::uint32_t marginalizations_{0U};
  std::uint32_t gnss_associated_{0U};
  std::uint32_t gnss_rejected_{0U};
  std::uint32_t downweighted_outliers_{0U};
  double maximum_pair_skew_ms_{0.0};
  double maximum_solve_time_us_{0.0};
};

}  // namespace daily_robotics_2026_09_29

#endif  // DAILY_ROBOTICS_2026_09_29__FIXED_LAG_SOLVER_HPP_
