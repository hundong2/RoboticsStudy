# 고정 상한 LiDAR Deskew 파이프라인

LiDAR deskew는 점마다 `t_i=stamp+i×time_increment`의 pose를 적용하는 문제다. 점마다 TF를 조회하면 cache 탐색, mutex, 예외, timeout 비용도 점 수만큼 증가한다. 짧은 sweep에서 운동이 부드럽다는 가정이 허용되면 시작/끝 pose만 exact-time으로 조회하고 내부 점 pose를 보간해 TF 호출 횟수를 상수로 만들 수 있다.

상한은 코드와 운영 지표에 함께 나타나야 한다. 예를 들어 `beams<=181`, `ray_steps<=100`, `TF lookups=3`, `lookup timeout<=5 ms`, 고정 `std::array` map처럼 적는다. 크기 초과 입력은 잘라서 조용히 쓰기보다 거절 카운터로 드러내야 한다. TF dropout도 이전 pose 재사용으로 숨기지 말고 drop/fallback 정책을 명시한다.

이 구조가 hard RT를 자동 보장하지는 않는다. TF Buffer/Listener, ROS 로그, 메시지 벡터, DDS publish, 일반 allocator와 Linux scheduler는 비결정적일 수 있다. 제품에서는 센서 수신·TF lookup·publish를 non-RT 경로에 두고, preallocated snapshot을 RT 계산 스레드로 넘기는 구조와 WCET/queueing trace가 필요하다.

참고: [ROS 2 TF2 time tutorial](https://docs.ros.org/en/jazzy/Tutorials/Intermediate/Tf2/Learning-About-Tf2-And-Time-Cpp.html), [ROS 2 real-time programming demo](https://docs.ros.org/en/ros2_documentation/jazzy/Tutorials/Demos/Real-Time-Programming.html).
