# ROS 2 Services & Lifecycle — 누적 노트

## Service를 선택하는 기준

Service는 **짧게 끝나며 결과가 필요한 요청/응답**에 적합하다. 설정 조회, 센서 영점 조정 요청, 상태 초기화가 대표적이다.

```text
Topic   : 연속 데이터 스트림, 발행자와 구독자 분리
Service : 짧은 Request/Response, 취소·진행률 없음
Action  : 오래 걸리는 Goal, Feedback/Result/Cancel 필요
```

Service callback에서 긴 I/O나 제어 동작을 수행하면 같은 Executor의 Timer/Subscription을 막을 수 있다. 오래 걸릴 가능성이 있으면 Action이나 작업 큐로 넘긴다. `async_send_request()`를 쓰면 client가 응답을 기다리며 Executor thread를 차단하지 않는다.

## 사용자 정의 `.srv`

```text
# Request
float64 value
---
# Response
bool accepted
string message
```

`rosidl_generate_interfaces()`가 C++/Python 타입과 middleware type support를 생성한다. C++ target은 생성된 typesupport target과 연결해야 한다.

## Lifecycle의 목적

관리 노드는 생성됐다고 즉시 동작하지 않는다. 외부 supervisor가 다음 상태 전이를 명시적으로 승인한다.

```text
UNCONFIGURED --configure--> INACTIVE --activate--> ACTIVE
      ^                        |
      +--------cleanup---------+
```

- `on_configure`: 파라미터 검증, 메모리 확보, Publisher/Subscription 생성
- `on_activate`: 하드웨어 출력과 LifecyclePublisher 활성화
- `on_deactivate`: 출력을 안전하게 중지하되 자원은 유지
- `on_cleanup`: configure에서 만든 자원을 해제
- `on_error`: 안전 상태로 복구하거나 Finalized로 이동

## 설계 원칙

1. 생성자에서 하드웨어를 움직이지 않는다.
2. `on_configure()`는 실패 가능 준비를 수행하고 성공/실패를 명확히 반환한다.
3. `ACTIVE`가 아니면 기능 Service와 센서 callback이 상태를 바꾸지 않게 한다.
4. 외부 manager는 프로세스 시작 순서가 아니라 ChangeState 응답을 기준으로 다음 노드를 활성화한다.
5. 시스템 종료은 활성화의 역순으로 `deactivate→cleanup→shutdown`한다.

## 연결 실습

- [`../../daily_robotics/2026-09-06/src/lifecycle_manager.cpp`](../../daily_robotics/2026-09-06/src/lifecycle_manager.cpp)
- [`../../daily_robotics/2026-09-06/src/lifecycle_ekf_node.cpp`](../../daily_robotics/2026-09-06/src/lifecycle_ekf_node.cpp)

## 레퍼런스

- [ROS 2 interfaces: topics, services, actions](https://docs.ros.org/en/jazzy/How-To-Guides/Topics-Services-Actions.html)
- [ROS 2 managed node design](https://design.ros2.org/articles/node_lifecycle.html)
