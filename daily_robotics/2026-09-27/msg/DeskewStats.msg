# 처리한 PointCloud2와 같은 시각/프레임을 사용한다.
std_msgs/Header header
uint32 input_points
uint32 output_points
uint32 imu_samples
uint32 rejected_scans
uint32 truncated_scans
float64[3] gyro_bias_rad_s
float64 rotation_sigma_rad
float64 callback_us
