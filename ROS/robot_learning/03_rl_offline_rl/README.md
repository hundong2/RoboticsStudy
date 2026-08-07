# 03. Reinforcement Learning과 Offline RL

목표는 reward를 이용해 policy를 개선하는 RL과, 이미 수집된 dataset만으로 policy를 학습하는 Offline RL의 차이를 이해하는 것입니다.

## 강의 목표

- MDP, return, value, Q-function, policy gradient를 설명한다.
- PPO, SAC 같은 online RL의 핵심 차이를 이해한다.
- Offline RL에서 distribution shift와 overestimation 문제가 왜 중요한지 설명한다.
- BC, offline RL, online fine-tuning의 연결 전략을 설계한다.

## 핵심 개념

| 개념 | 질문 |
|---|---|
| reward | 무엇을 잘했다고 볼 것인가? |
| exploration | 보지 못한 상태를 어떻게 탐색할 것인가? |
| value function | 이 상태나 action이 장기적으로 좋은가? |
| offline dataset | 수집된 행동 분포 밖으로 나가도 되는가? |
| conservative learning | 데이터에 없는 action을 과대평가하지 않는가? |

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | MDP 그림 그리기 | state/action/reward diagram |
| 2 | tabular Q-learning | 작은 gridworld |
| 3 | policy gradient toy | loss derivation note |
| 4 | PPO/SAC paper note | 알고리즘 비교표 |
| 5 | replay buffer 구현 | sample batch |
| 6 | offline dataset split | train/val/test split |
| 7 | BC vs offline RL baseline | metric table |
| 8 | OOD action 분석 | action histogram |
| 9 | reward hacking 사례 분석 | failure note |
| 10 | offline-to-online fine-tuning 설계 | safety plan |

## 실습 과제

1. GridWorld 또는 간단한 continuous control task를 만든다.
2. random, medium, expert dataset을 각각 만든다.
3. BC와 offline RL baseline을 비교한다.
4. dataset quality가 성능에 미치는 영향을 표로 정리한다.

## 통과 기준

- online RL과 offline RL의 데이터 수집 비용 차이를 설명한다.
- offline RL에서 policy가 dataset 밖 action을 고르는 위험을 설명한다.
- 실제 로봇에서 무제한 exploration이 불가능한 이유를 안전 관점에서 설명한다.
