# RT-1: Robotics Transformer for Real-World Control at Scale

- 원문: https://arxiv.org/abs/2212.06817
- 프로젝트: https://robotics-transformer1.github.io/
- 저자: Anthony Brohan et al.

## 왜 읽는가

로봇 행동 데이터를 대규모로 모아 Transformer 기반 정책으로 학습하는 접근을 보여준다. VLA 이전에 `로봇 행동을 sequence modeling 문제로 보는 방식`을 이해하기 좋다.

## 핵심 번역식 요약

- 다양한 실제 로봇 task 데이터를 모아 하나의 정책 모델을 학습한다.
- 입력은 이미지와 자연어 instruction 등이고, 출력은 로봇 action이다.
- 데이터 규모, task 다양성, 모델 크기가 일반화에 중요하다는 점을 실험한다.
- 실시간 제어가 가능하도록 action을 효율적으로 token화한다.

## 읽을 때 볼 것

- 어떤 데이터를 수집했는가
- action을 어떻게 표현했는가
- generalization을 어떻게 평가했는가
- 실패 사례가 무엇인가

## 확인 질문

- imitation learning과 어떤 관련이 있는가?
- task-agnostic data가 왜 중요한가?
- 실제 로봇 데이터 수집이 왜 병목인가?
- RT-1은 VLM인가, VLA인가, 아니면 그 중간인가?
