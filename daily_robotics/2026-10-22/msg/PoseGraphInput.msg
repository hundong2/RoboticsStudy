std_msgs/Header header
uint32 sequence

# 이 메시지는 실습용으로 최대 12개 pose를 한 번에 전달한다.
# pose_count까지만 유효하고, 나머지 고정 배열 원소는 사용하지 않는다.
uint8 pose_count
float64[12] initial_x
float64[12] initial_y
float64[12] initial_yaw
float64[12] truth_x
float64[12] truth_y
float64[12] truth_yaw

# 연속 odometry edge는 최대 11개다.
uint8 odometry_count
uint8[11] odometry_from
uint8[11] odometry_to
float64[11] odometry_dx
float64[11] odometry_dy
float64[11] odometry_dyaw

# loop closure 후보는 최대 3개이며, expected_inlier는 auditor 전용 정답이다.
uint8 loop_count
uint8[3] loop_from
uint8[3] loop_to
float64[3] loop_dx
float64[3] loop_dy
float64[3] loop_dyaw
bool[3] loop_expected_inlier
