# 고정 연산량 계획과 ROS 메시지 경계

`std::array<Sample, 101>`은 수학 계산 반복 횟수를 고정한다. 그러나 `JointTrajectory.points`, 점별 `positions/velocities/accelerations`는 벡터이며 직렬화와 DDS 발행도 힙 할당, 락, 네트워크 지연을 일으킬 수 있다. 따라서 이 예제는 bounded *계산*을 보여주지만 hard RT 루프 자체가 아니다.

실기 설계에서는 사전 할당된 RT 루프에 수치 상태 갱신만 두고, 메시지 조립/기록/운영자 보고는 비RT 스레드로 넘긴다. ring buffer 용량·초과 정책과 deadlines를 명시하고 플랫폼에서 WCET, jitter, 메모리 할당, 컨트롤러 보간 결과를 별도로 측정한다. `ros2 bag record`는 유용한 관측 도구이나 디스크 쓰기 부하를 고려해야 한다. [2026-09-19 실습](../../daily_robotics/2026-09-19/README.md).
