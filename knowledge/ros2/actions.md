# ROS 2 Actions — 장시간 작업의 Goal/Feedback/Result

## 한 문장 정의

Action은 **시간이 걸리고 진행률과 취소가 필요한 작업**을 Goal 수락/거부, Feedback, Result, Cancel로 모델링하는 ROS 2 인터페이스다.

## Topic·Service·Action 선택

| 인터페이스 | 적합한 문제 | 완료 응답 | 진행률 | 취소 |
|---|---|---:|---:|---:|
| Topic | 연속 센서·상태 스트림 | 없음 | 별도 설계 | 없음 |
| Service | 짧은 질의·설정 | 있음 | 없음 | 없음 |
| Action | 이동·조작·계획 같은 장시간 작업 | 있음 | 있음 | 있음 |

Action 정의는 두 개의 `---`로 Goal, Result, Feedback을 나눈다.

```text
<goal fields>
---
<result fields>
---
<feedback fields>
```

## 서버 수명주기

1. `handle_goal`: 수치 범위, 안전 조건, 동시 Goal 정책을 검사한다.
2. `handle_accepted`: 실행 상태를 초기화하고 작업을 시작한다.
3. 실행 중 `publish_feedback`: 진행률과 상태를 보낸다.
4. `handle_cancel`: 취소 가능 여부를 판단하고 안전 정지를 요청한다.
5. `succeed`, `abort`, `canceled`: 정확히 하나의 종료 상태와 Result를 전달한다.

## 실무 설계 체크리스트

- Goal validation을 장치 명령 전에 수행한다.
- 동시에 허용할 Goal 수와 선점(preemption) 정책을 명시한다.
- Cancel 응답 수락과 물리적 정지는 같은 순간이 아님을 문서화한다.
- Feedback 빈도는 관측성 요구와 통신/CPU 비용을 함께 고려한다.
- 서버 재시작·통신 단절·중복 Goal의 실패 모드를 정한다.
- Action 콜백에서 긴 계산을 직접 수행해 executor를 막지 않는다.
- RT 제어라면 Goal 객체를 RT 루프에 직접 넘기지 말고 고정 크기 command/state로 경계를 만든다.

## 관련 실습

- `daily_robotics/2026-09-05`: custom `DriveDistance.action`, 단일 Goal 예약, Feedback/Result, RT 루프 handoff
- `daily_robotics/2026-09-25`: 3축 비영점 경계조건 Action, goal tolerance, 500 Hz fixed plan/50 Hz feedback 분리, 독립 추종 감사

## 2026-09-25 확장 — 궤적 Action의 허용오차와 RT 경계

- Action Goal은 원하는 경계상태와 허용오차를 전달하고, 고주기 제어용 계수/command는 서버의 비 RT callback에서 미리 만든다.
- Feedback 주기는 control 주기보다 낮아도 된다. 500 Hz 제어를 관측하기 위해 500 Hz DDS Feedback을 강제하면 직렬화 비용과 네트워크 부하만 커질 수 있다.
- Goal reservation은 validation 뒤 원자적으로 수행해 거의 동시에 온 두 요청이 모두 수락되지 않게 한다.
- Result의 `SUCCEEDED`는 단순 시간 만료가 아니라 terminal tolerance 만족을 뜻해야 한다.
- Cancel 수락과 제한 준수 정지는 별도 상태다. 실장비에서는 braking trajectory 또는 drive-level stop의 완료를 Result에 반영한다.
- 자세한 경계 패턴은 [`knowledge/realtime/action_control_boundary.md`](../realtime/action_control_boundary.md)에 정리한다.

## 참고 자료

- [ROS 2 Jazzy: Topics, Services, Actions](https://docs.ros.org/en/jazzy/How-To-Guides/Topics-Services-Actions.html)
- [ROS 2 C++ Action Server/Client tutorial](https://docs.ros.org/en/ros2_documentation/rolling/Tutorials/Intermediate/Writing-an-Action-Server-Client/Cpp.html)
