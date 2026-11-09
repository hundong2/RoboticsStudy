# Bounded NDT kernel의 결과와 계산 건전성 telemetry다.
std_msgs/Header header
uint32 sequence
float64 estimated_x
float64 estimated_y
float64 estimated_yaw
float64 mean_mahalanobis_cost
uint32 valid_points
uint32 occupied_cells
uint32 iterations
uint32 rejected_points
uint32 solve_failures
float64 minimum_hessian_pivot
float64 kernel_time_us
bool converged
