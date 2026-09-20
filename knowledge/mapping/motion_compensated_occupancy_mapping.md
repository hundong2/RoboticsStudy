# 운동 보정 점유격자 지도화

회전식 LiDAR의 scan은 한 시각의 snapshot이 아니다. `i`번째 range를 첫 pose에 그대로 투영하면 로봇의 sweep 중 운동이 공간 왜곡으로 바뀐다. 광선별 acquisition pose `T_map_laser(t_i)`를 사용해 endpoint와 free ray를 map frame으로 옮기는 과정이 motion compensation 또는 deskew다.

2D에서는 `map←base` pose와 고정 `base←laser` extrinsic을 합성한다.

```text
t_ml = t_mb + R(yaw_mb) t_bl
yaw_ml = yaw_mb + yaw_bl
p_hit = t_ml + r [cos(yaw_ml+theta), sin(yaw_ml+theta)]ᵀ
```

pose uncertainty가 크면 endpoint occupied evidence를 약하게 만드는 것이 안전하다. 한 근사로 `sigma_end²=sigma_xy²+(r sigma_yaw)²+sigma_range²`를 쓰고 map resolution과 비교해 weight를 정할 수 있다. 그러나 셀 독립성, 연속 beam 상관, 동적 물체, grazing incidence를 무시하면 수치가 엄밀한 확률은 아니다. 반드시 실제 bag에서 calibration하고 raw/deskew map의 벽 두께·residual을 비교한다.

선형 pose 보간은 짧고 부드러운 sweep의 교육용 출발점이다. 급가속·3D 운동에는 IMU/wheel odometry 기반 continuous trajectory, quaternion interpolation, per-point timestamp, time-offset calibration이 필요하다.

참고: [LOAM 원문 페이지](https://www.ri.cmu.edu/publications/loam-lidar-odometry-and-mapping-in-real-time/), [ROS 2 LaserScan 정의](https://github.com/ros2/common_interfaces/blob/jazzy/sensor_msgs/msg/LaserScan.msg).
