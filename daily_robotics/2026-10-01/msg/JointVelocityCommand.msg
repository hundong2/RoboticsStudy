# Header.stamp는 이 명령을 계산한 ROS 시각이고 frame_id는 관절 명령에는 비워 둔다.
std_msgs/Header header
# 서로 다른 Topic의 명령과 진단을 독립 감사 노드가 정확히 짝짓기 위한 단조 증가 번호다.
uint64 sequence
# joint_1, joint_2, joint_3 순서의 안전 제한 적용 관절 속도 [rad/s]다.
float64[3] velocity_rad_s
