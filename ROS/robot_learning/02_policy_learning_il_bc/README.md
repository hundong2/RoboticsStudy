# 02. Imitation Learning과 Behavior Cloning

목표는 demonstration으로부터 policy를 학습하는 방법을 익히는 것입니다. 이 단계에서는 reward를 직접 설계하지 않고, 사람이 보여준 행동을 따라 하도록 학습합니다.

## 강의 목표

- Behavior Cloning을 supervised learning으로 구현한다.
- image-conditioned policy와 state-conditioned policy를 비교한다.
- multi-modal action 문제를 이해한다.
- DAgger, ACT, Diffusion Policy, VQ-BeT가 왜 등장했는지 설명한다.

## 핵심 개념

| 방법 | 핵심 아이디어 | 장점 | 한계 |
|---|---|---|---|
| Behavior Cloning | observation에서 expert action을 예측 | 단순하고 강력한 baseline | covariate shift |
| DAgger | learner가 방문한 상태에서 expert label 추가 | distribution shift 완화 | expert query 필요 |
| ACT | action chunk를 예측해 장기 의존성 완화 | real robot manipulation에 강함 | 데이터/구현 복잡도 |
| Diffusion Policy | action distribution을 diffusion으로 생성 | multi-modal action 처리 | sampling cost |
| VQ-BeT | action을 latent token으로 이산화 | multimodal behavior 표현 | codebook/토큰 설계 필요 |

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | BC 문제 정의 | observation/action schema |
| 2 | state-only BC 구현 | train/eval curve |
| 3 | image encoder 추가 | CNN feature map shape |
| 4 | action normalization | scale 전후 비교 |
| 5 | covariate shift 실험 | 실패 trajectory |
| 6 | sequence window 적용 | history length ablation |
| 7 | action chunking | chunk size 비교 |
| 8 | mixture density head 또는 latent action 조사 | multimodal action note |
| 9 | robomimic/LeRobot 구조 읽기 | framework comparison |
| 10 | BC mini project 발표 | 실패 분석 1장 |

## 실습 과제

1. synthetic 2D navigation demonstration 100개를 만든다.
2. MLP BC와 sequence BC를 비교한다.
3. closed-loop rollout에서 expert trajectory와 learner trajectory 차이를 측정한다.
4. 실패한 상태를 10개 모아 원인을 분류한다.

## 통과 기준

- open-loop MSE가 낮아도 closed-loop 실패가 날 수 있음을 설명한다.
- demonstration 품질, coverage, action noise가 policy 성능에 미치는 영향을 분석한다.
- BC를 항상 첫 baseline으로 두는 이유를 설명한다.
