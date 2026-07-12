# 04. Message, Service, Action 설계

ROS 시스템 설계에서 가장 중요한 결정은 어떤 데이터를 어떤 통신 방식으로 흘릴지 정하는 것입니다.

## Message

Message는 topic으로 오가는 데이터 형식입니다.

```text
geometry_msgs/msg/Twist
  Vector3 linear
  Vector3 angular
```

`Twist`는 선속도와 각속도를 표현합니다. 차동 구동 로봇에서는 보통 `linear.x`와 `angular.z`만 사용합니다.

## Service

Service는 요청과 응답이 한 번씩 오갑니다.

```text
Request  -> 저장할 파일 이름
Response -> 성공 여부, 메시지
```

예시는 맵 저장, 모드 변경, 캘리브레이션 시작처럼 결과가 즉시 판단되는 작업입니다.

## Action

Action은 오래 걸리는 목표를 처리합니다.

```text
Goal     -> 이동할 목적지
Feedback -> 현재 진행률
Result   -> 성공/실패와 최종 상태
```

Nav2의 목적지 이동, 로봇 팔의 pick and place, 장시간 스캔 작업에 적합합니다.

## 설계 원칙

- 센서 스트림은 topic으로 보냅니다.
- 명령도 반복 제어면 topic으로 보냅니다.
- 설정 변경처럼 짧고 명확한 요청은 service로 보냅니다.
- 취소, 진행률, 결과가 필요한 긴 작업은 action으로 만듭니다.
- message field 이름은 물리량과 단위를 짐작할 수 있게 만듭니다.

## 나쁜 설계 예

| 나쁜 선택 | 문제 |
|---|---|
| 이미지 데이터를 service로 요청 | 지연과 대역폭 문제가 커짐 |
| 목적지 이동을 단일 topic으로만 처리 | 취소, 진행률, 실패 원인 표현이 어려움 |
| 모든 설정을 코드 상수로 고정 | 실험마다 재빌드가 필요 |

## 실습

1. `geometry_msgs/msg/Twist`를 확인합니다.
2. `std_srvs/srv/Trigger`를 확인합니다.
3. `nav2_msgs/action/NavigateToPose` 구조를 확인합니다.
4. 각 구조가 topic, service, action 중 어디에 맞는지 설명합니다.
