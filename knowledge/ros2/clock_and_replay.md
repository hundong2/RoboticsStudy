# ROS 2 시각과 재생 계약

`SystemTime`은 OS 벽시계, `SteadyTime`은 간격/하드웨어 타임아웃, `ROSTime`은 `use_sim_time=true`에서 `/clock`을 따른다. `/clock`이 아직 없을 때 ROS time 0은 초기화되지 않았다는 신호일 수 있다. 센서 `Header.stamp`는 측정 시각이며, 콜백 도착 시각을 적분 간격으로 쓰면 재생 속도와 네트워크 지연이 계산 결과를 바꾼다.

시계가 뒤로 점프하는 bag 반복/seek에서는 누적 적분, 필터 상태, 타이머 지연, TF 버퍼의 epoch가 달라질 수 있다. 처리 정책을 명시하자: ① 역행 감지와 이전 상태 초기화, ② 새 첫 샘플을 앵커로 설정, ③ 재생 시작마다 동일 파라미터와 입력 순서를 적용, ④ 출력 stamp와 frame을 검증. 센서별 시각 동기화 일반론은 [time_synchronization.md](../sensors/time_synchronization.md)에 누적한다.

`/clock`을 발행하는 노드에 그 자신의 sim-time 타이머를 걸면 시작 시각이 없을 때 진도가 멈출 수 있다. 벽시계로 시뮬레이터를 구동하고 소비자만 sim time을 사용하는 것이 작은 실습에서 안전하다. sim time은 재생 논리의 시각이지 CPU deadline을 보장하는 RT 스케줄러가 아니다.

참고: [ROS 2 Clock and Time 설계](https://design.ros2.org/articles/clock_and_time.html), [rosbag2](https://github.com/ros2/rosbag2).
