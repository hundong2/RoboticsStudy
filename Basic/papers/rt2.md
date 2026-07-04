# RT-2: Vision-Language-Action Models Transfer Web Knowledge to Robotic Control

- 원문: https://arxiv.org/abs/2307.15818
- PMLR: https://proceedings.mlr.press/v229/zitkovich23a.html
- 프로젝트: https://robotics-transformer2.github.io/

## 왜 읽는가

VLA라는 용어를 이해하는 핵심 논문이다. 웹 규모 vision-language 지식을 로봇 행동으로 전이하려는 문제의식이 잘 드러난다.

## 핵심 번역식 요약

- vision-language model을 로봇 trajectory 데이터와 함께 fine-tuning한다.
- 자연어 응답과 로봇 action을 같은 token 형식으로 다루려 한다.
- 웹 데이터에서 배운 시각/언어 지식이 로봇의 새로운 물체, 지시, 간단한 추론으로 전이될 수 있음을 보인다.
- VLA는 perception과 language understanding을 action generation에 연결하는 모델 범주다.

## 읽을 때 볼 것

- action을 text token처럼 다룬다는 아이디어
- co-fine-tuning이 왜 필요한지
- emergent capability라고 부르는 사례
- 평가가 실제 로봇 trial 기반이라는 점

## 확인 질문

- VLM과 VLA의 결정적 차이는 무엇인가?
- 웹 지식이 로봇 행동에 도움이 되는 이유는 무엇인가?
- action tokenization의 장점과 위험은 무엇인가?
- 실제 제품에 바로 쓰기 어려운 이유는 무엇인가?
