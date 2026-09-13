# DDS Content Filter와 Deadline Supervision

## ContentFilteredTopic

`rclcpp::SubscriptionOptions::content_filter_options`는 SQL `WHERE`와 유사한 필터 식과 `%0` 형식 parameter를 RMW에 전달한다. 지원 DDS는 reader 쪽에서 불필요한 sample을 제거해 callback/역직렬화 부하를 줄일 수 있다.

반드시 다음을 지킨다.

1. `subscription->is_cft_enabled()`를 시작 로그와 진단에 남긴다.
2. 미지원 RMW를 위한 software filter 또는 기능 축소 정책을 둔다.
3. 필터는 성능 최적화로 취급하고 safety condition의 유일한 방어선으로 쓰지 않는다.
4. expression field와 parameter quoting을 실제 RMW 조합에서 integration test한다.

## Deadline supervisor

메시지의 ROS stamp만 검사하면 발행자 clock 정지와 `/clock` 변화에 취약하다. 수신 순간의 `steady_clock`을 저장하고 `now-last_seen > deadline`을 독립 timer로 검사한다.

안전 경로와 진단 경로는 callback group/executor를 분리한다. 하지만 executor 분리만으로 OS 우선순위 역전, page fault, DDS thread 지연이 사라지지는 않는다. PREEMPT_RT, CPU affinity, memory locking, tracing과 함께 최악 지연을 검증해야 한다.

참고: [ROS 2 content filtering example](https://github.com/ros2/examples/blob/rolling/rclcpp/topics/minimal_subscriber/content_filtering.cpp)
