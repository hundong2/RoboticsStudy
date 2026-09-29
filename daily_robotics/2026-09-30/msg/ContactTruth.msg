# 시뮬레이터 전용 ground truth. 실제 제품에서는 안전 감독기에 연결하지 않는다.
std_msgs/Header header
uint8 mode
float64 friction_coefficient
float64 normal_force_n
float64 requested_tangential_force_n
float64 actual_tangential_force_n
float64 slip_speed_mps
