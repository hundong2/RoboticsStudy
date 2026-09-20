# ROS 2 LaserScan 계약

`sensor_msgs/msg/LaserScan`은 평면 라이다 한 스캔이다. `header.stamp`는 **첫 번째 광선**의 취득 시각, `header.frame_id`는 광선의 좌표계다. 양의 각도는 +Z축 주위 반시계 방향이며, i번째 방향은 `angle_min+i×angle_increment`이다. 광선별 시각은 `stamp+i×time_increment`로 근사할 수 있고, 센서가 움직이면 이를 이용한 운동 보정이 필요하다.

`ranges[i]`의 단위는 m이다. 정의상 `range_min`보다 작거나 `range_max`보다 큰 값은 버린다. NaN/Inf와 무반사의 표현은 드라이버별로 확인한다. `angle_max`와 배열 길이가 일치하는지, `angle_increment>0`인지, `frame_id`가 실제 TF와 연결되는지도 입력 검증에 포함한다.

센서 데이터에는 일반적으로 `rclcpp::SensorDataQoS()`를 쓴다. Jazzy 기본값은 keep-last 5, best-effort, volatile이다. 구독자/발행자의 신뢰성·내구성이 호환되어야 데이터가 흐른다. 지도를 늦게 접속하는 소비자에게 최신 스냅샷으로 주려면 별도의 reliable/transient-local QoS가 적합하다.

2026-09-18 실습은 라이다가 map 원점에 고정되어 있어 `frame_id=map`이라고 직접 둔다. 움직이는 로봇에서는 각 스캔의 시각에 맞는 TF로 광선을 지도 좌표에 투영해야 한다. `range==range_max`를 free-only로 다루는 규칙은 그 실습 시뮬레이터의 선택이다.

참고: [Jazzy LaserScan 원본 메시지](https://github.com/ros2/common_interfaces/blob/jazzy/sensor_msgs/msg/LaserScan.msg), [Jazzy SensorDataQoS](https://docs.ros.org/en/jazzy/p/rclcpp/generated/classrclcpp_1_1SensorDataQoS.html).

## 2026-09-21 확장 — rolling scan과 TF cache

움직이는 센서에서는 `header.stamp`와 마지막 광선 시각 `stamp+(N-1)×time_increment`가 모두 TF cache 안에 있어야 한다. 최신 TF(`TimePointZero`)를 동적 pose에 쓰면 모든 광선을 도착 시각의 pose로 잘못 투영한다. exact-time lookup 실패를 과거 최신 pose로 조용히 대체하면 map 왜곡을 숨길 수 있으므로, 스캔 drop·제한된 extrapolation·odometry fallback 중 하나를 명시하고 카운터로 관측한다.

광선마다 TF lookup을 반복하는 대신 sweep 시작/끝 pose를 가져와 보간할 수 있다. 이는 bounded 계산에 유리하지만 constant-velocity에 가까운 짧은 구간이라는 모델 가정이다. 가속이 큰 플랫폼에서는 IMU/encoder trajectory와 per-point time을 사용하고, 보간 오차를 pose covariance 또는 map quality metric으로 감시한다.
