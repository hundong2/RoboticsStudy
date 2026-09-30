# 명령과 동일한 stamp를 사용해 한 번의 수치 커널 실행 결과임을 나타낸다.
std_msgs/Header header
uint64 sequence
# 감사 노드가 Jacobian을 독립 재계산할 수 있도록 사용한 관절 각도를 싣는다.
float64[3] joint_position_rad
# 원래 Cartesian 속도 목표와 실제 J*q_dot 결과 [m/s]다.
float64[2] desired_xy_m_s
float64[2] achieved_xy_m_s
# 불가능한 목표를 방향 보존하며 줄인 비율 s, 0 <= s <= 1이다.
float64 task_scale
# ||J*q_dot - s*x_dot_desired||_2 [m/s]다.
float64 scaled_residual_m_s
# J의 가장 큰 특이값 / 가장 작은 특이값이다.
float64 condition_number
# steady_clock으로 관측한 한 번의 수치 커널 실행 시간이며 WCET 보장은 아니다.
float64 callback_us
# bit i가 1이면 i번 관절이 속도 한계에 닿았다.
uint8 saturated_mask
bool singularity_guard_active
bool numerical_fallback_active
