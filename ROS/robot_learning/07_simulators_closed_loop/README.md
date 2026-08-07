# 07. 시뮬레이터 기반 대규모 학습과 Closed-Loop 평가

목표는 학습된 정책을 단순 validation loss가 아니라 실제 실행 루프에서 평가하는 방법을 익히는 것입니다.

## 강의 목표

- Open-loop 평가와 closed-loop 평가의 차이를 설명한다.
- Gazebo, Isaac Sim, CARLA, Webots를 목적에 맞게 선택한다.
- success rate, collision rate, SPL, intervention count, latency를 측정한다.
- 시뮬레이터에서 수집한 데이터가 실제 로봇에 어떤 bias를 만드는지 분석한다.

## 프레임워크 선택

| 프레임워크 | 적합한 실습 |
|---|---|
| Gazebo | ROS 2/Nav2/mobile robot closed-loop |
| Webots | 교육용 GUI, 빠른 센서 실습 |
| Isaac Sim | GPU sensor, synthetic data, Isaac ROS, photorealistic scene |
| CARLA | 자율주행 차량, 도로/교통 scenario |
| MuJoCo/PyBullet/Drake | 동역학, 제어, RL 알고리즘 검증 |

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | open-loop vs closed-loop 예제 | 차이 설명 |
| 2 | Gazebo 또는 Webots baseline 실행 | topic list |
| 3 | rollout logger 작성 | trajectory log |
| 4 | success/collision metric 정의 | metric spec |
| 5 | random seed와 scenario split | eval split |
| 6 | latency 측정 | inference timing |
| 7 | failure replay | rosbag 또는 replay log |
| 8 | CARLA/Isaac Sim 자료 조사 | tool selection memo |
| 9 | 대규모 평가 설계 | batch evaluation plan |
| 10 | 평가 리포트 작성 | closed-loop report |

## 실습 과제

1. 하나의 navigation task에 대해 open-loop metric과 closed-loop metric을 둘 다 정의한다.
2. 시뮬레이터 scenario 10개를 train/validation/test로 나눈다.
3. 실패를 재현할 수 있는 log 또는 rosbag 저장 전략을 작성한다.
4. 사람이 개입한 횟수를 metric으로 포함한다.

## 통과 기준

- validation loss가 낮아도 closed-loop 성공률이 낮을 수 있음을 설명한다.
- 시뮬레이터 randomization과 test split을 구분한다.
- 평가 결과에 confidence interval 또는 반복 실행 통계를 포함한다.
