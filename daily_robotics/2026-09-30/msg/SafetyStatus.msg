# 감독기의 결정 근거를 감사 가능하게 만드는 고정 크기 상태 메시지.
std_msgs/Header header
uint8 state
float64 requested_force_n
float64 allowed_force_n
float64 conservative_limit_n
float64 estimate_age_ms
uint32 consecutive_good
uint32 consecutive_bad
uint32 trip_count
