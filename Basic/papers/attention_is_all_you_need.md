# Attention Is All You Need

- 원문: https://arxiv.org/abs/1706.03762
- 저자: Ashish Vaswani et al.
- 발표: NeurIPS 2017

## 왜 읽는가

VLM, VLA, RT-1, RT-2 같은 최근 로봇 AI 모델은 대부분 Transformer 계열 구조 위에 있다. 이 논문은 Transformer의 출발점이다.

## 핵심 번역식 요약

- RNN/CNN 없이 attention만으로 sequence 모델을 만들 수 있다.
- self-attention은 입력 토큰들 사이의 관계를 직접 계산한다.
- multi-head attention은 여러 관점에서 관계를 본다.
- positional encoding은 순서 정보를 넣기 위해 필요하다.
- 병렬화가 쉬워 대규모 학습에 유리하다.

## 로봇 관점에서 보기

로봇에서는 sequence가 텍스트만이 아니다. 이미지 패치, 센서 토큰, 행동 토큰, 상태 토큰도 sequence로 만들 수 있다. RT 계열 모델은 이 발상을 로봇 행동 데이터에 적용한다.

## 확인 질문

- query, key, value는 각각 어떤 역할인가?
- self-attention과 cross-attention의 차이는 무엇인가?
- positional encoding이 없으면 어떤 정보가 사라지는가?
- 로봇 행동을 token으로 표현한다는 말은 무엇을 의미하는가?
