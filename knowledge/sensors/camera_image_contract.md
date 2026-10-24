# ROS 2 Camera Image/Calibration 계약

## 같은 acquisition을 묶는 필드

- `Image.header.stamp`와 대응하는 `CameraInfo.header.stamp`는 image acquisition 시각이어야 한다.
- 두 message의 `frame_id`는 같은 camera optical frame이어야 한다.
- optical frame 관례는 원점이 optical center, `+x` right, `+y` down, `+z` forward다.
- topic 도착 순서는 계약이 아니다. 정확한 stamp, bounded tolerance, 또는 camera driver가 제공하는 동기화 정책으로 결합한다.

## Image storage

- `encoding`은 channel 의미와 byte layout을 정한다. `mono8`, `rgb8`, `bgr8`, depth encoding을 혼동하지 않는다.
- `step`은 한 행의 byte 수다. 항상 `width * channels`라고 가정하면 padding/ROI에서 깨진다.
- 최소한 `data.size() >= step * height`와 endian/encoding 지원 여부를 검증한다.
- raw Bayer image와 rectified image를 같은 camera model로 해석하지 않는다.

## CameraInfo geometry

\[
K=\begin{bmatrix}f_x&0&c_x\\0&f_y&c_y\\0&0&1\end{bmatrix}
\]

- `K`: raw/distorted image intrinsic
- `D`: distortion coefficients와 `distortion_model`
- `R`: rectification rotation
- `P`: rectified/processed image의 3×4 projection matrix
- `K[0]==0`은 uncalibrated camera를 나타낼 수 있으므로 geometry 연산을 중지하거나 명시적 fallback을 사용한다.
- binning과 ROI가 바뀌면 유효 projection geometry도 바뀐다. 원본 calibration width/height만 확인해서는 부족하다.

## QoS와 수명

- 고속 image stream은 최신 표본 우선 `SensorDataQoS`가 흔하다.
- calibration snapshot은 `Reliable + Transient Local + KeepLast(1)`이 늦게 시작한 subscriber에 유용하다.
- publisher/subscriber QoS compatibility를 검증한다. image drop, DDS history drop, application processing queue drop을 따로 계측한다.

## 제품 체크

- hardware acquisition timestamp인가, host reception timestamp인가?
- rolling-shutter row time/exposure midpoint가 필요한가?
- intrinsic/extrinsic이 temperature/focus/zoom과 함께 변하는가?
- rectification map과 CameraInfo version이 원자적으로 바뀌는가?
- invalid calibration/stale camera info 때 downstream이 fail closed 하는가?

## 참고

- [ROS 2 Jazzy Image.msg](https://github.com/ros2/common_interfaces/blob/jazzy/sensor_msgs/msg/Image.msg)
- [ROS 2 Jazzy CameraInfo.msg](https://github.com/ros2/common_interfaces/blob/jazzy/sensor_msgs/msg/CameraInfo.msg)
