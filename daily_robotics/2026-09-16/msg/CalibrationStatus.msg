# 이 메시지는 온라인 보정기의 결과와 계산 예산을 독립 auditor로 전달한다.
std_msgs/Header header
float64 estimated_offset_ms
float64 residual_rmse
float64 fused_yaw_rate
uint32 matched_pairs
uint32 imu_samples
uint32 lidar_samples
uint32 search_candidates
uint64 solve_time_us
