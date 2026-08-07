# 00. 기초 수학, Python, 로봇 데이터

목표는 딥러닝 로봇 정책 학습에 필요한 최소 기초를 빠짐없이 다지는 것입니다. 여기서 말하는 기초는 수학을 많이 아는 상태가 아니라, 로봇 데이터와 학습 코드에서 어떤 값이 어떤 의미인지 놓치지 않는 상태입니다.

## 강의 목표

- Python 함수, 클래스, 모듈, 가상환경을 다룬다.
- tensor shape을 읽고 batch, time, channel, action dimension을 구분한다.
- 확률분포, log likelihood, KL divergence, entropy를 직관적으로 설명한다.
- pose, velocity, action, observation, reward, done을 구분한다.
- ROS topic과 학습 dataset row가 어떻게 대응되는지 이해한다.

## 핵심 개념

| 개념 | 로봇 학습에서의 의미 |
|---|---|
| observation | 이미지, LiDAR, proprioception, language instruction |
| state | 시스템의 내부 상태 또는 환경 상태 |
| action | 속도, 조인트 목표, waypoint, discrete command |
| trajectory | 시간 순서가 있는 observation-action-reward 묶음 |
| policy | observation을 action으로 바꾸는 함수 |
| distribution | 같은 observation에서도 여러 action이 가능함을 표현 |
| loss | 모델 출력과 목표 행동의 차이를 줄이는 기준 |

## 10일 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | Python list/dict/function/class 복습 | `RobotState` 클래스 |
| 2 | NumPy 배열 shape 변환 | `(T, H, W, C)`와 `(B, C, H, W)` 변환 노트 |
| 3 | PyTorch tensor와 autograd | 선형회귀 직접 학습 |
| 4 | 확률분포와 sampling | Gaussian action sampling |
| 5 | KL divergence/entropy 직관 | 두 Gaussian 비교 코드 |
| 6 | pose, quaternion, yaw | 2D pose 변환 함수 |
| 7 | camera image preprocessing | resize/normalize pipeline |
| 8 | trajectory schema 설계 | JSONL 또는 Parquet schema |
| 9 | ROS bag과 dataset 대응 | topic-to-dataset mapping |
| 10 | 미니 시험 | observation/action/reward 설명 |

## 과제

1. `observation`, `action`, `reward`, `done` 필드를 가진 trajectory 샘플 20개를 만든다.
2. action을 deterministic 값과 Gaussian distribution으로 각각 표현한다.
3. "왜 로봇 정책은 MSE만으로 부족할 수 있는가?"를 10문장으로 설명한다.

## 통과 기준

- tensor shape 오류를 보고 어느 dimension이 틀렸는지 찾는다.
- image observation과 action sequence를 하나의 batch로 묶을 수 있다.
- BC, RL, Offline RL에서 dataset이 어떻게 달라지는지 말할 수 있다.
