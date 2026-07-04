# PaLM-E: An Embodied Multimodal Language Model

- 원문: https://arxiv.org/abs/2303.03378
- 프로젝트: https://palm-e.github.io/
- 저자: Danny Driess et al.

## 왜 읽는가

VLM이 단순히 이미지를 설명하는 수준을 넘어, 로봇 상태와 센서 입력을 언어 모델에 연결하는 방향을 보여준다. `embodied`라는 단어가 왜 중요한지 이해하기 좋다.

## 핵심 번역식 요약

- 언어 모델에 시각, 상태 추정, 센서 정보를 함께 넣는다.
- 로봇 planning, visual question answering, captioning을 하나의 multimodal 모델에서 다룬다.
- 여러 embodiment와 여러 task를 함께 학습하면 전이가 가능해질 수 있다.
- 핵심 문제는 단어와 실제 세계의 관측을 연결하는 grounding이다.

## 읽을 때 볼 것

- 입력이 단순 텍스트가 아니라 sensor/state token을 포함한다는 점
- 로봇 문제와 일반 VQA 문제가 함께 학습된다는 점
- 실제 low-level control 전체를 해결한다기보다 embodied reasoning 쪽에 무게가 있다는 점

## 확인 질문

- embodied model은 일반 VLM과 무엇이 다른가?
- grounding이란 무엇인가?
- planning과 control은 어떻게 다른가?
- PaLM-E가 직접 모터 명령까지 모두 해결한다고 보면 안 되는 이유는 무엇인가?
