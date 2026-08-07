# 08. Sim-to-Real과 실제 로봇 배포

목표는 시뮬레이션에서 학습한 policy를 실제 로봇에 배포할 때 생기는 gap을 체계적으로 줄이는 것입니다.

## 강의 목표

- sim-to-real gap의 원인을 sensor, dynamics, latency, calibration, domain shift로 나눈다.
- domain randomization과 system identification을 비교한다.
- safety controller와 learned policy의 책임 경계를 설계한다.
- 실제 로봇 배포 전 checklist와 rollback 계획을 만든다.

## 주요 gap

| 원인 | 예시 | 대응 |
|---|---|---|
| sensor gap | 노이즈, motion blur, exposure | augmentation, calibration |
| dynamics gap | 바퀴 미끄러짐, actuator delay | system ID, controller tuning |
| latency gap | camera/inference/control 지연 | timestamp, profiling |
| visual gap | texture, lighting, weather | domain randomization |
| safety gap | 예외 상황, 사람/장애물 | safety layer, emergency stop |

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | sim-to-real gap taxonomy | gap table |
| 2 | camera calibration 개념 | calibration checklist |
| 3 | latency budget 작성 | timing diagram |
| 4 | domain randomization 설계 | randomization config |
| 5 | safety controller 설계 | override rule |
| 6 | dry-run mode 설계 | no-motion inference log |
| 7 | staged rollout 계획 | speed limit schedule |
| 8 | failure recovery | e-stop/rollback plan |
| 9 | real data fine-tuning 계획 | data collection protocol |
| 10 | 배포 리뷰 | deployment readiness report |

## 실습 과제

1. 시뮬레이터와 실제 로봇 사이 차이를 20개 적는다.
2. 가장 위험한 gap 5개에 대해 mitigation을 적는다.
3. "정지 명령을 언제 우선할 것인가" safety rule을 작성한다.
4. policy 배포 전후 latency를 측정하는 방법을 설계한다.

## 통과 기준

- learned policy가 항상 최종 제어 권한을 가져서는 안 되는 이유를 설명한다.
- 실제 로봇 실험에서 속도 제한, 공간 제한, 사람 격리, emergency stop이 필요한 이유를 설명한다.
- sim data와 real data를 섞어 fine-tuning할 때 data provenance를 기록한다.
