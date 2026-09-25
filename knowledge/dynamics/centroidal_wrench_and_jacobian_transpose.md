# Centroidal Wrench와 Jacobian Transpose

## 목적

여러 접촉력이 로봇 전체의 힘/모멘트를 어떻게 만들고, 한 접촉력이 관절 토크로 어떻게 전달되는지 연결한다.

## 접촉력의 합

기준점 O에 대해 접점 i의 위치가 `r_i`, 접촉력이 `f_i`이면 전체 wrench는 다음이다.

```text
F = sum_i f_i
tau_O = sum_i (r_i × f_i) + sum_i tau_i
```

기준점을 바꾸면 force는 같아도 moment가 달라진다. 따라서 wrench 메시지에는 frame과 기준점 계약이 반드시 필요하다.

## Jacobian transpose

접점 속도와 관절 속도의 관계가 `x_dot = J(q) q_dot`이면 가상일 보존에서 다음이 나온다.

```text
tau^T dq = f^T dx
dx = J dq
tau = J^T f
```

이 식은 “접촉력 때문에 관절이 받는 generalized force”를 말한다. 중력, 관성, Coriolis, actuator friction까지 자동으로 보상해 주는 완전한 torque command 식은 아니다.

## 실무 체크리스트

- force와 moment의 단위가 각각 N, N·m인지 확인한다.
- `frame_id`뿐 아니라 moment 기준점도 문서화한다.
- 환경이 로봇에 가하는 힘인지 로봇이 환경에 가하는 힘인지 부호를 고정한다.
- Jacobian의 row 순서와 wrench component 순서를 일치시킨다.
- singularity 근처에서는 condition number와 task 축소/damping 정책을 둔다.
- actuator torque limit을 `J^T f` 뒤에서 다시 검사한다.

## 관련 실습

- `daily_robotics/2026-09-26`: 평면 centroidal wrench, 두 발 힘 분배, 2R `J^T f`
- `daily_robotics/2026-09-23`: 접촉 임피던스와 에너지 감사

## 참고

- [Khatib, Operational Space Formulation, 1987](https://khatib.stanford.edu/publications/pdfs/Khatib_1987_RA.pdf)
- [ROS 2 Jazzy `geometry_msgs/Wrench`](https://docs.ros.org/en/jazzy/p/geometry_msgs/msg/Wrench.html)
