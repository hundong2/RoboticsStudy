# 이 통계는 제어 명령 경로가 아니라 저주기 독립 감사 경로에서만 사용한다.
std_msgs/Header header
uint64 sequence
float64[2] desired_xy_m
float64[2] actual_xy_m
float64 task_error_m
float64 nullspace_leakage
float64 task_inertia_det
float64 damping_lambda
float64 torque_norm_nm
float64 callback_us
bool singularity_guard_active
