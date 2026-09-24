# 비 RT 발행 경계에서 만들어진 추종 관측 시각이다.
std_msgs/Header header
# 5차 다항식이 만든 목표 상태.
float64[3] desired_position_rad
float64[3] desired_velocity_rad_s
float64[3] desired_acceleration_rad_s2
# feed-forward + PD 모의 plant가 만든 실제 상태.
float64[3] actual_position_rad
float64[3] actual_velocity_rad_s
# 이 표본에서 세 관절 중 가장 큰 위치 오차.
float64 max_position_error_rad
# 500 Hz 절대 주기 deadline보다 늦게 깨어난 시간. WCET가 아니라 관측된 wake-up lateness다.
int64 wakeup_lateness_ns
# Goal 시작 뒤 계산한 제어 tick 수.
uint64 control_ticks
# true면 이 frame이 Action 종료 직전의 마지막 상태다.
bool terminal
