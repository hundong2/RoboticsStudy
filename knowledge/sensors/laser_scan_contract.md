# ROS 2 LaserScan 계약

`sensor_msgs/msg/LaserScan`은 평면 라이다 한 스캔이다. `header.stamp`는 **첫 번째 광선**의 취득 시각, `header.frame_id`는 광선의 좌표계다. 양의 각도는 +Z축 주위 반시계 방향이며, i번째 방향은 `angle_min+i×angle_increment`이다. 광선별 시각은 `stamp+i×time_increment`로 근사할 수 있고, 센서가 움직이면 이를 이용한 운동 보정이 필요하다.

`ranges[i]`의 단위는 m이다. 정의상 `range_min`보다 작거나 `range_max`보다 큰 값은 버린다. NaN/Inf와 무반사의 표현은 드라이버별로 확인한다. `angle_max`와 배열 길이가 일치하는지, `angle_increment>0`인지, `frame_id`가 실제 TF와 연결되는지도 입력 검증에 포함한다.

센서 데이터에는 일반적으로 `rclcpp::SensorDataQoS()`를 쓴다. Jazzy 기본값은 keep-last 5, best-effort, volatile이다. 구독자/발행자의 신뢰성·내구성이 호환되어야 데이터가 흐른다. 지도를 늦게 접속하는 소비자에게 최신 스냅샷으로 주려면 별도의 reliable/transient-local QoS가 적합하다.

2026-09-18 실습은 라이다가 map 원점에 고정되어 있어 `frame_id=map`이라고 직접 둔다. 움직이는 로봇에서는 각 스캔의 시각에 맞는 TF로 광선을 지도 좌표에 투영해야 한다. `range==range_max`를 free-only로 다루는 규칙은 그 실습 시뮬레이터의 선택이다.

참고: [Jazzy LaserScan 원본 메시지](https://github.com/ros2/common_interfaces/blob/jazzy/sensor_msgs/msg/LaserScan.msg), [Jazzy SensorDataQoS](https://docs.ros.org/en/jazzy/p/rclcpp/generated/classrclcpp_1_1SensorDataQoS.html).
