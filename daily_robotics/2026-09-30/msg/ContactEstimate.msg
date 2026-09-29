# 접촉 추정기의 고정 크기 출력. mode: 0=AIR, 1=STABLE, 2=SLIP.
std_msgs/Header header
uint8 mode
float64 normal_force_n
float64 tangential_force_n
float64 slip_speed_mps
float64 contact_probability
float64 velocity_variance
float64 friction_ratio
float64 friction_mean
float64 friction_stddev
float64 friction_lower_bound
float64 friction_margin_n
float64 pair_skew_ms
float64 callback_time_us
uint32 accepted_samples
uint32 rejected_pairs
