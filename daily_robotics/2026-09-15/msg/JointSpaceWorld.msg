std_msgs/Header header

# 2축 관절 공간(q1, q2)의 시작/목표 자세다. 배열 순서는 joint_names의 순서와 같다.
float64[2] start
float64[2] goal

# 각 Vector3는 관절 공간의 원형 금지 영역이다: x=q1 중심, y=q2 중심, z=반지름.
# 실제 로봇에서는 PlanningScene의 충돌 검사를 쓰지만, 오늘은 RRT 핵심을 보이기 위해 단순화한다.
geometry_msgs/Vector3[] obstacles
