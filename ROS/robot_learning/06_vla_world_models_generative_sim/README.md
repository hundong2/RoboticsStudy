# 06. VLA, World Model, 생성형 시뮬레이션

목표는 Vision-Language-Action 모델, world model, 생성형 시뮬레이션을 로봇 정책 학습 파이프라인 안에서 이해하는 것입니다.

## 강의 목표

- VLA 모델이 image, language, action을 어떻게 연결하는지 설명한다.
- World Model이 미래 observation/reward/state를 예측하는 역할을 이해한다.
- 3D Gaussian Splatting과 diffusion 기반 scenario generation의 쓰임을 구분한다.
- foundation model을 로봇 policy로 쓸 때 안전과 검증 문제가 왜 커지는지 설명한다.

## 핵심 개념

| 주제 | 역할 | 실습 방향 |
|---|---|---|
| VLA | 언어 지시와 시각 관찰을 action으로 변환 | instruction-action dataset schema |
| World Model | action 이후 미래를 예측 | latent rollout error 측정 |
| 3DGS | 실제 장면을 빠르게 novel-view rendering | view synthesis와 data augmentation |
| Diffusion scenario generation | 환경/상황 다양성 생성 | rare case 생성 계획 |
| Robot foundation model | 대규모 데이터로 일반화 | downstream adaptation 설계 |

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | VLA input/output schema 설계 | JSON schema |
| 2 | language instruction template 작성 | command set |
| 3 | CLIP/vision embedding 개념 정리 | embedding note |
| 4 | action tokenization 설계 | token/action table |
| 5 | RT-1/RT-2 논문 읽기 | model card |
| 6 | world model rollout toy | prediction error plot |
| 7 | latent dynamics 모델 | rollout visualization |
| 8 | 3DGS 활용 시나리오 설계 | scene capture plan |
| 9 | diffusion scenario generation 설계 | rare event list |
| 10 | safety/eval checklist | VLA risk register |

## 실습 과제

1. "문 앞으로 이동해", "빨간 표식을 찾아", "장애물을 피해" 같은 instruction 30개를 만든다.
2. 각 instruction을 observation/action schema와 연결한다.
3. world model이 예측해야 할 변수를 정의한다.
4. 생성형 시뮬레이션으로 늘리고 싶은 rare case 20개를 작성한다.

## 통과 기준

- VLA 모델을 그대로 실제 로봇에 배포하면 위험한 이유를 설명한다.
- World Model의 rollout error가 장기 계획에서 누적되는 문제를 설명한다.
- 3DGS는 policy 자체가 아니라 scene reconstruction/data generation 도구로 볼 수 있음을 설명한다.
