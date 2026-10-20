# 감사 노드가 자원 상한과 접촉 전이를 독립 검증할 수 있는 고정 필드 진단이다.
std_msgs/Header header
uint32 processed_samples
uint32 contact_updates
uint32 rejected_updates
uint32 contact_transitions
uint32 queue_drops
uint32 invalid_samples
uint32 max_samples_per_cycle
uint8 contact_state
float64 innovation_nis
float64 height_variance
float64 accel_bias_mps2
float64 max_cycle_time_us
bool bounded_contract_ok
