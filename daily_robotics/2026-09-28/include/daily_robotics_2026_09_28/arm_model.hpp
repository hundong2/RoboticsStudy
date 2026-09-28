#pragma once

#include <array>
#include <cmath>

namespace daily_robotics_2026_09_28::model
{

// 동적 크기의 Eigen 행렬 대신 크기가 컴파일 시점에 고정된 std::array를 사용한다.
// 이 선택은 수치 커널 안에서 heap 할당이 일어날 여지를 줄이고, 반복 횟수도 3으로 고정한다.
using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;
using Mat2 = std::array<std::array<double, 2>, 2>;
using Mat3 = std::array<std::array<double, 3>, 3>;
using Mat23 = std::array<std::array<double, 3>, 2>;
using Mat32 = std::array<std::array<double, 2>, 3>;

constexpr Vec3 kLinkLengthM{0.60, 0.45, 0.30};
constexpr Vec3 kLinkMassKg{2.00, 1.50, 0.80};
constexpr Vec3 kViscousDamping{0.40, 0.30, 0.20};
constexpr double kGravityMps2 = 9.81;

// 누적 관절각 theta_k = q_0 + ... + q_k를 계산한다.
// 평면 직렬 로봇의 각 링크 방향은 개별 q가 아니라 이 누적각으로 정해진다.
inline Vec3 cumulative_angles(const Vec3 & q)
{
  return Vec3{q[0], q[0] + q[1], q[0] + q[1] + q[2]};
}

// 3R 평면 팔의 순기구학 p = sum(l_k [cos(theta_k), sin(theta_k)])이다.
// 즉 관절 공간 q를 작업 공간의 말단 위치 (x, y)로 바꾼다.
inline Vec2 forward_kinematics(const Vec3 & q)
{
  const Vec3 theta = cumulative_angles(q);
  Vec2 position{0.0, 0.0};
  for (std::size_t link = 0; link < 3; ++link) {
    position[0] += kLinkLengthM[link] * std::cos(theta[link]);
    position[1] += kLinkLengthM[link] * std::sin(theta[link]);
  }
  return position;
}

// J(q)는 x_dot = J(q) q_dot을 만족하는 2x3 기하 자코비안이다.
// 열 j는 관절 j를 아주 조금 움직였을 때 말단 x/y가 얼마나 변하는지를 뜻한다.
inline Mat23 jacobian(const Vec3 & q)
{
  const Vec3 theta = cumulative_angles(q);
  Mat23 result{};
  for (std::size_t joint = 0; joint < 3; ++joint) {
    for (std::size_t link = joint; link < 3; ++link) {
      result[0][joint] -= kLinkLengthM[link] * std::sin(theta[link]);
      result[1][joint] += kLinkLengthM[link] * std::cos(theta[link]);
    }
  }
  return result;
}

// x_ddot = J q_ddot + J_dot q_dot에서 두 번째 항만 계산한다.
// 각 링크 누적 각속도 omega_k의 구심가속도
// -l_k*[cos(theta_k), sin(theta_k)]*omega_k^2를 모두 더한 식이다.
inline Vec2 jacobian_dot_times_velocity(const Vec3 & q, const Vec3 & q_dot)
{
  const Vec3 theta = cumulative_angles(q);
  Vec2 bias_acceleration{0.0, 0.0};
  double cumulative_velocity = 0.0;
  for (std::size_t link = 0; link < 3; ++link) {
    cumulative_velocity += q_dot[link];
    const double centripetal =
      kLinkLengthM[link] * cumulative_velocity * cumulative_velocity;
    bias_acceleration[0] -= centripetal * std::cos(theta[link]);
    bias_acceleration[1] -= centripetal * std::sin(theta[link]);
  }
  return bias_acceleration;
}

// 점 질량 병진 에너지와 각 링크 중심 관성 에너지를 합쳐 M(q)를 만든다.
// T = 1/2 q_dot^T M(q) q_dot이라는 로봇 동역학 수식의 M을 코드로 옮긴 부분이다.
inline Mat3 mass_matrix(const Vec3 & q)
{
  const Vec3 theta = cumulative_angles(q);
  Mat3 mass{};

  for (std::size_t body = 0; body < 3; ++body) {
    std::array<Vec2, 3> center_jacobian{};
    for (std::size_t joint = 0; joint <= body; ++joint) {
      for (std::size_t segment = joint; segment < body; ++segment) {
        center_jacobian[joint][0] -=
          kLinkLengthM[segment] * std::sin(theta[segment]);
        center_jacobian[joint][1] +=
          kLinkLengthM[segment] * std::cos(theta[segment]);
      }
      center_jacobian[joint][0] -=
        0.5 * kLinkLengthM[body] * std::sin(theta[body]);
      center_jacobian[joint][1] +=
        0.5 * kLinkLengthM[body] * std::cos(theta[body]);
    }

    const double inertia_about_com =
      kLinkMassKg[body] * kLinkLengthM[body] * kLinkLengthM[body] / 12.0;
    for (std::size_t row = 0; row <= body; ++row) {
      for (std::size_t col = 0; col <= body; ++col) {
        const double translational = kLinkMassKg[body] *
          (center_jacobian[row][0] * center_jacobian[col][0] +
          center_jacobian[row][1] * center_jacobian[col][1]);
        // 평면 회전 링크에서 body 이하의 모든 관절은 body 각속도에 계수 1로 기여한다.
        mass[row][col] += translational + inertia_about_com;
      }
    }
  }
  return mass;
}

// g(q) = dV/dq를 계산한다. 여기서 V는 각 링크 질량중심의 중력 위치에너지다.
// viscous 항까지 더한 bias는 M(q)q_ddot + bias(q,q_dot) = tau에 사용한다.
inline Vec3 dynamics_bias(const Vec3 & q, const Vec3 & q_dot)
{
  const Vec3 theta = cumulative_angles(q);
  Vec3 bias{};
  for (std::size_t body = 0; body < 3; ++body) {
    for (std::size_t joint = 0; joint <= body; ++joint) {
      double dy_dq = 0.0;
      for (std::size_t segment = joint; segment < body; ++segment) {
        dy_dq += kLinkLengthM[segment] * std::cos(theta[segment]);
      }
      dy_dq += 0.5 * kLinkLengthM[body] * std::cos(theta[body]);
      bias[joint] += kLinkMassKg[body] * kGravityMps2 * dy_dq;
    }
  }
  for (std::size_t joint = 0; joint < 3; ++joint) {
    bias[joint] += kViscousDamping[joint] * q_dot[joint];
  }
  return bias;
}

// 3x3 역행렬을 닫힌형으로 계산한다. 행렬 크기와 연산 수가 고정되어 반복 상한이 명확하다.
// det가 너무 작으면 false를 반환하여 수치적으로 위험한 나눗셈을 차단한다.
inline bool inverse3(const Mat3 & matrix, Mat3 & inverse, double & determinant)
{
  const double a = matrix[0][0];
  const double b = matrix[0][1];
  const double c = matrix[0][2];
  const double d = matrix[1][0];
  const double e = matrix[1][1];
  const double f = matrix[1][2];
  const double g = matrix[2][0];
  const double h = matrix[2][1];
  const double i = matrix[2][2];

  determinant = a * (e * i - f * h) - b * (d * i - f * g) +
    c * (d * h - e * g);
  if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-9) {
    return false;
  }

  const double scale = 1.0 / determinant;
  inverse = Mat3{{
    {{(e * i - f * h) * scale, (c * h - b * i) * scale, (b * f - c * e) * scale}},
    {{(f * g - d * i) * scale, (a * i - c * g) * scale, (c * d - a * f) * scale}},
    {{(d * h - e * g) * scale, (b * g - a * h) * scale, (a * e - b * d) * scale}}
  }};
  return true;
}

// 2x2 작업공간 관성 행렬의 역행렬용 보조 함수다.
inline bool inverse2(const Mat2 & matrix, Mat2 & inverse, double & determinant)
{
  determinant = matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
  if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-12) {
    return false;
  }
  const double scale = 1.0 / determinant;
  inverse = Mat2{{
    {{matrix[1][1] * scale, -matrix[0][1] * scale}},
    {{-matrix[1][0] * scale, matrix[0][0] * scale}}
  }};
  return true;
}

}  // namespace daily_robotics_2026_09_28::model
