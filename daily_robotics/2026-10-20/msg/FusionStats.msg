# 이 메시지는 고주기 자세 결과와 분리된 저주기 진단 계약이다.
# Header.stamp는 이 통계가 설명하는 최신 fused_pose의 측정 시각이다.
builtin_interfaces/Time stamp
uint32 sequence
uint8 active_states
uint8 linear_dimension
uint8 gauss_newton_iterations
uint16 pending_absolute_factors
uint32 delayed_factors_inserted
uint32 too_old_factors_dropped
uint32 solve_failures
float64 last_solve_microseconds
float64 max_solve_microseconds
