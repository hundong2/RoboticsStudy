# TF2 좌표계 변환 핵심

## 왜 frame을 메시지와 함께 보내는가

로봇의 숫자는 좌표계가 없으면 의미가 없다. `(1, 0, 0)`은 LiDAR 앞 1 m일 수도 있고 지도 원점 오른쪽 1 m일 수도 있다. `geometry_msgs/*Stamped`의 `header.frame_id`와 `header.stamp`는 각각 “어디에서”와 “언제” 측정했는지를 보존한다.

## target ← source 표기

TF2의 다음 호출은 source frame의 데이터를 target frame 표현으로 바꾸는 변환을 찾는다.

```cpp
buffer.lookupTransform(target_frame, source_frame, time);
```

점에 적용되는 식은 다음과 같다.

```text
p_target = R_target_source · p_source + t_target_source
```

여러 변환은 회전과 평행이동 순서를 지켜 합성한다.

```text
T_map_lidar = T_map_base · T_base_lidar
p_map = T_map_lidar · p_lidar
```

행렬 곱 순서를 바꾸면 일반적으로 다른 결과가 나온다.

## 정적 TF와 동적 TF

- `/tf_static`: 센서 장착 위치처럼 변하지 않는 관계. 한 번 발행하고 transient-local durability로 늦은 listener에게도 전달한다.
- `/tf`: 주행 중의 `map→base_link`, 관절 운동처럼 시간에 따라 바뀌는 관계. 실제 갱신 주기에 맞춰 계속 발행한다.
- frame tree는 loop가 없어야 하고 각 child는 하나의 parent만 가져야 한다.

## timestamp 원칙

- `stamp=0`/`tf2::TimePointZero`: 버퍼가 가진 최신 공통 시각의 변환을 사용한다. 시각화나 간단한 데모에 편하다.
- 실제 센서 stamp: 해당 측정이 발생한 시각의 자세를 사용한다. 움직이는 로봇의 센서 융합에는 이것이 원칙이다.
- 미래 외삽 오류: 센서 메시지가 최신 TF보다 앞선 시각을 요청할 때 발생할 수 있다.
- 과거 외삽 오류: TF buffer 보존 구간보다 오래된 측정을 요청할 때 발생할 수 있다.

실무에서는 `tf2_ros::MessageFilter`로 필요한 TF가 준비될 때까지 메시지를 제한된 큐에 보관한다. 무한 대기나 무한 큐는 오래된 센서 데이터가 제어 명령으로 뒤늦게 들어오는 원인이 된다.

## 쿼터니언 주의점

- ROS geometry message 순서는 `(x, y, z, w)`다.
- 단위 쿼터니언이어야 순수 회전을 나타낸다. 계산 뒤 normalize가 필요한지 확인한다.
- `q`와 `-q`는 같은 회전을 나타내므로 성분 부호만 비교해서 자세가 다르다고 판단하면 안 된다.
- 점 회전의 쿼터니언 표현은 convention에 따라 `q p q⁻¹` 또는 반대로 보일 수 있다. TF2 API를 사용하고 frame 방향을 테스트로 고정하는 편이 안전하다.

## 디버깅 체크

```bash
ros2 run tf2_tools view_frames
ros2 run tf2_ros tf2_echo map lidar
ros2 topic echo /tf --once
ros2 topic echo /tf_static --once
```

변환 오류가 나면 frame 철자, tree 연결, timestamp, clock source(`/use_sim_time`), publisher 주기를 이 순서로 확인한다.

## 참고

- [ROS 2 Jazzy tf2_ros API](https://docs.ros.org/en/jazzy/p/tf2_ros/)
- [ROS 2 TF2 broadcaster tutorial](https://docs.ros.org/en/kilted/Tutorials/Intermediate/Tf2/Writing-A-Tf2-Broadcaster-Cpp.html)
- [ROS 2 TF2 listener tutorial](https://docs.ros.org/en/kilted/Tutorials/Intermediate/Tf2/Writing-A-Tf2-Listener-Cpp.html)
