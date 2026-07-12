# 08. MoveIt2와 조작 로봇

MoveIt2는 로봇 팔의 motion planning, 충돌 검사, planning scene, trajectory execution을 담당하는 ROS 2 생태계의 핵심 조작 프레임워크입니다.

## 핵심 개념

| 개념 | 의미 |
|---|---|
| Planning Group | 함께 움직이는 조인트 묶음 |
| End Effector | 그리퍼 또는 작업 도구 |
| Planning Scene | 로봇과 주변 장애물 상태 |
| IK | 목표 pose를 만족하는 joint angle 계산 |
| Trajectory | 시간에 따른 joint 목표 |
| Controller | trajectory를 실제 joint 명령으로 변환 |

## 실습 흐름

1. URDF/xacro로 로봇 팔 모델을 준비합니다.
2. SRDF로 planning group과 end effector를 정의합니다.
3. RViz MotionPlanning 패널에서 목표 pose를 지정합니다.
4. 충돌 없는 trajectory를 plan합니다.
5. fake controller 또는 실제 controller로 execute합니다.

## 디버깅 포인트

- joint limit이 실제 로봇과 맞는가?
- collision mesh가 너무 복잡하거나 부정확하지 않은가?
- planning frame과 end effector frame이 일관적인가?
- controller 이름이 MoveIt2 설정과 일치하는가?

## 통과 기준

- "계획은 성공했지만 실행이 안 됨" 상황에서 controller 설정을 확인한다.
- IK 실패와 collision 실패를 구분한다.
- 실제 로봇 적용 전 속도/가속도 제한과 비상 정지 절차를 검토한다.
