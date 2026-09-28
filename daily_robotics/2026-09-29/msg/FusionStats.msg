# 이 메시지는 고정 지연 smoother의 수치 품질과 실행 상한을 감사 노드로 전달한다.
std_msgs/Header header
uint8 window_size
uint8 irls_iterations
uint32 marginalizations
uint32 gnss_associated
uint32 gnss_rejected
uint32 downweighted_outliers
float64 minimum_gnss_weight
float64 maximum_pair_skew_ms
float64 latest_variance_x
float64 latest_variance_y
float64 pivot_condition_proxy
float64 maximum_solve_time_us
