# 05. TF2, URDF, Robot Modeling

로봇은 여러 좌표계를 동시에 사용합니다. 카메라는 카메라 좌표계를, LiDAR는 LiDAR 좌표계를, 바퀴 제어는 base 좌표계를 기준으로 동작합니다. TF2는 이 관계를 시간과 함께 관리합니다.

## 핵심 프레임

| 프레임 | 의미 |
|---|---|
| `map` | 전역 지도 기준 좌표 |
| `odom` | 바퀴/IMU 적분 기반 지역 좌표 |
| `base_link` | 로봇 몸체 기준 좌표 |
| `base_footprint` | 바닥에 투영한 로봇 기준 좌표 |
| `laser` | LiDAR 기준 좌표 |
| `camera_link` | 카메라 기준 좌표 |

## 대표 TF 흐름

```text
map -> odom -> base_link -> laser
                         -> camera_link
```

- `map -> odom`은 localization 또는 SLAM이 보정합니다.
- `odom -> base_link`는 wheel odometry 또는 EKF가 제공합니다.
- `base_link -> sensor`는 URDF의 고정 joint로 표현합니다.

## URDF

URDF는 로봇의 링크와 조인트를 XML로 표현하는 형식입니다.

```xml
<link name="base_link" />
<joint name="laser_joint" type="fixed">
  <parent link="base_link" />
  <child link="laser" />
</joint>
```

## 실습 위치

[projects/03_tf_urdf_robot_model](../projects/03_tf_urdf_robot_model/README.md)에 차동 구동 로봇 예제가 있습니다.

## 통과 기준

- RViz Fixed Frame 오류를 보고 어떤 TF가 빠졌는지 추론한다.
- URDF의 `link`와 `joint` 관계를 트리로 그릴 수 있다.
- 센서 메시지의 `header.frame_id`가 TF 트리에 존재해야 하는 이유를 설명한다.
