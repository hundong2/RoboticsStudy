# ROS 2 Diagnostic Health Contract

## 목적

`diagnostic_msgs/DiagnosticArray`는 로봇 부품의 상태를 운영자·로그·진단 aggregator에 전달하는 표준 메시지다. 제어 Topic과 달리 진단은 “무엇이 잘못됐고 어떤 근거가 있는가”를 설명해야 한다.

## 최소 계약

- `header.stamp`: 이 진단 스냅샷을 만든 시각
- `status[].name`: 계층적인 안정 이름(예: `actuator/left_wheel/fault_supervisor`)
- `hardware_id`: 실제 장치/벤치 식별자
- `level`: `OK`, `WARN`, `ERROR`, `STALE`
- `message`: 사람이 즉시 이해할 한 줄 요약
- `values`: 확률, sample age, 카운터, 실행시간처럼 판단 근거가 되는 key-value

`level`만 내보내면 원인 분석과 회귀 검증이 어렵다. threshold, 현재 값, 누적 횟수, 마지막 신선도처럼 재현 가능한 근거를 함께 보낸다.

## 실시간 경계

Diagnostic 메시지는 문자열과 가변 길이 배열을 포함하므로 hard-RT hot path에서 매 주기 생성하기에 부적합하다. 권장 구조는 다음과 같다.

```text
고주기 고정 크기 계산 → scalar/fixed snapshot → 저주기 DiagnosticArray 직렬화
```

스냅샷 전달 방식은 단일 executor, atomic seqlock, realtime buffer 중 실제 concurrency 모델에 맞춰 선택한다. 진단 publish 실패가 제어 계산을 막지 않게 한다.

## 참고

- https://github.com/ros2/common_interfaces/blob/rolling/diagnostic_msgs/msg/DiagnosticStatus.msg
- https://github.com/ros2/common_interfaces/blob/rolling/diagnostic_msgs/msg/DiagnosticArray.msg
