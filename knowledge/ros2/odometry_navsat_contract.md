# ROS 2 Odometry와 NavSatFix 계약

## `nav_msgs/msg/Odometry`

- `header.stamp`: pose가 유효한 측정 시각
- `header.frame_id`: pose가 표현된 기준 frame. 일반적으로 `odom` 또는 `map`
- `child_frame_id`: 움직이는 body frame. 일반적으로 `base_link`
- `pose`: `header.frame_id → child_frame_id`의 pose
- `twist`: message 정의상 `child_frame_id`에서 표현되는 속도
- pose/twist covariance: `[x,y,z,roll,pitch,yaw]` 순서의 6×6 row-major 배열

Pose와 twist가 서로 다른 frame convention을 쓴다는 점을 놓치기 쉽다. TF tree와 중복 publish할 때는 동일 timestamp에서 transform 값이 모순되지 않는지도 확인한다.

## `sensor_msgs/msg/NavSatFix`

- latitude/longitude는 WGS84 degree, altitude는 WGS84 ellipsoid 기준 meter다.
- `status.status < STATUS_FIX`면 위치 factor를 만들지 않는다.
- `position_covariance`는 ENU(East, North, Up) 순서 3×3 row-major다.
- covariance type이 `UNKNOWN`이면 0을 “완벽한 센서”로 해석하지 않는다. driver별 fallback 또는 reject 정책이 필요하다.
- `header.frame_id`는 GNSS 안테나 frame을 가리킨다. lever arm을 무시하면 회전 시 위치 bias가 생긴다.

## Global 좌표를 local factor로 바꾸기

작은 지역의 교육용 근사는 다음과 같다.

\[
x_E \approx R\cos(\phi_0)(\lambda-\lambda_0),\qquad
y_N \approx R(\phi-\phi_0)
\]

각도는 radian이다. 넓은 범위, UTM zone 경계, 극지방, 정밀 고도에는 GeographicLib/PROJ 또는 검증된 ROS 변환 노드를 사용한다. datum과 `map` frame 원점을 bag/설정과 함께 기록한다.

## 검증 체크리스트

- stamp가 driver 수신 시각인지 receiver measurement 시각인지 확인
- 위경도 degree/radian, covariance m² 단위 확인
- antenna→base_link lever arm TF와 calibration version 기록
- status/covariance unknown/NaN 처리 시험
- datum 재시작, `/use_sim_time`, clock jump, bag replay 시험
- GNSS innovation NIS와 실제 오차를 비교해 covariance scale 검증

## 참고

- [ROS 2 Jazzy nav_msgs/Odometry](https://docs.ros.org/en/jazzy/p/nav_msgs/msg/Odometry.html)
- [ROS 2 Jazzy sensor_msgs/NavSatFix](https://docs.ros.org/en/jazzy/p/sensor_msgs/msg/NavSatFix.html)
