# PointCloud2 필드와 점별 시간 계약

## 왜 별도 계약이 필요한가

ROS 2 `sensor_msgs/msg/PointCloud2`는 `fields`, `point_step`, `row_step`, `data`로 임의의 점 레이아웃을 표현한다. 표준은 x/y/z 외의 point timestamp 이름·타입·단위를 강제하지 않는다. 따라서 LiDAR deskew 소비자는 토픽 타입만 맞는다고 데이터를 해석해서는 안 된다.

## 소비자 체크리스트

1. `header.stamp`가 scan start/end/첫 packet 중 무엇인지 확인한다.
2. point timestamp field 이름(`time`, `t`, `timestamp`)과 타입(float seconds, uint32 ns 등)을 확인한다.
3. timestamp가 header 상대인지, 센서 boot 절대시각인지 확인한다.
4. `is_bigendian`, field offset/count/datatype, `point_step`, `row_step`, data 길이를 함께 검증한다.
5. point time이 scan duration 범위에 있고 단조적인지 검사하되, non-repetitive scan pattern은 별도 계약으로 다룬다.
6. IMU가 `[scan_start, scan_end]` 전체를 bracket하지 않으면 extrapolate하지 말고 reject/hold 정책을 적용한다.

## 안전한 C++ 접근

바이트 배열을 `reinterpret_cast<float*>`로 바로 역참조하면 정렬과 strict-aliasing 문제가 생길 수 있다. field offset으로 주소를 계산하고 `std::memcpy`로 스칼라를 읽는 방법이 이식성이 높다. 대량 변환에서는 `PointCloud2Iterator`나 검증된 point type adapter를 사용하되, 런타임 레이아웃 검사는 유지한다.

## 참고

- [PointCloud2](https://docs.ros.org/en/jazzy/p/sensor_msgs/msg/PointCloud2.html)
- [PointField](https://docs.ros.org/en/jazzy/p/sensor_msgs/msg/PointField.html)

