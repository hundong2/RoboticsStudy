# 07. SLAM, Localization, Nav2

자율주행 로봇의 핵심은 위치를 알고, 지도를 만들고, 목표까지 안전하게 이동하는 것입니다.

## 구성 요소

| 요소 | 역할 |
|---|---|
| SLAM | 센서로 지도를 만들면서 자기 위치를 추정 |
| Localization | 이미 있는 지도에서 현재 위치를 추정 |
| Costmap | 장애물과 이동 비용을 격자로 표현 |
| Planner | 전역/지역 경로를 계산 |
| Controller | 실제 속도 명령을 생성 |
| Behavior Tree | 실패 복구와 행동 순서를 관리 |

## Nav2 기본 흐름

```text
sensor data -> costmap -> planner -> controller -> /cmd_vel
                     ^                     |
                     |                     v
                   TF/odom <---------- robot motion
```

## 실습 위치

[projects/04_nav2_simulation_mission](../projects/04_nav2_simulation_mission/README.md)를 따라 시뮬레이션 기반 미션을 구성합니다.

## 디버깅 우선순위

1. `/tf`와 `/tf_static`이 완전한가?
2. `/odom`이 자연스럽게 증가하는가?
3. LiDAR topic의 frame id가 TF 트리에 있는가?
4. costmap에 장애물이 표시되는가?
5. planner는 path를 만들고 controller는 `/cmd_vel`을 내는가?

## 통과 기준

- SLAM 모드와 localization 모드의 차이를 설명한다.
- Nav2가 실패했을 때 TF, costmap, planner, controller 중 어디부터 볼지 판단한다.
- 실제 로봇에서 바퀴 미끄러짐과 센서 노이즈가 localization에 미치는 영향을 설명한다.
