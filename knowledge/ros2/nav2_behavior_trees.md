# Nav2 Behavior Tree 운용 패턴

## 책임 분리

- BT Navigator: 장기 navigation task의 흐름, 재시도, recovery, preemption을 조립한다.
- Global planner: 다른 위상 경로가 필요할 때 목표까지의 경로를 다시 찾는다.
- Local controller/optimizer: 현재 경로 주변의 짧은 시간 척도 변화에 반응한다.
- Safety supervisor: planner/controller 성공 여부와 독립적으로 정지 출력을 강제한다.

## 자주 쓰는 control node 직관

- `Sequence`: 앞 child가 성공한 뒤 다음 child로 간다.
- `Fallback`: 앞 child가 실패하면 다음 대안을 시도한다.
- `PipelineSequence`: 뒤 child가 실행 중이어도 앞 child를 다시 tick해 재계획 파이프라인을 만든다.
- `RecoveryNode`: 주 child 실패 시 recovery child를 실행하고 제한 횟수만큼 다시 시도한다.
- `RateController`: 비싼 planner tick 빈도를 controller tick보다 낮춘다.

## 실무 체크

BT XML은 정책이지 안전 인증서가 아니다. 각 leaf action의 timeout/cancel 의미, blackboard 데이터 수명, recovery 횟수, 최종 안전 상태를 함께 정의한다. 실패 원인을 숨기는 무한 재시도는 피하고, 같은 실패가 누적되면 operator 또는 상위 mission manager에 올린다.

참고: [Nav2 Behavior-Tree Navigator](https://docs.nav2.org/jazzy/configuration_and_development/configuration_guide/core_servers/configuring_bt_navigator/)
