# CLIP: Learning Transferable Visual Models From Natural Language Supervision

- 원문: https://arxiv.org/abs/2103.00020
- 저자: Alec Radford et al.
- 발표: ICML 2021

## 왜 읽는가

VLM의 가장 중요한 기초 중 하나다. 이미지와 텍스트를 같은 임베딩 공간에 놓고, 자연어로 시각 개념을 지정할 수 있게 만든다.

## 핵심 번역식 요약

- 이미지 encoder와 텍스트 encoder를 따로 둔다.
- 맞는 이미지-텍스트 쌍은 가깝게, 틀린 쌍은 멀게 학습한다.
- 학습 후에는 텍스트 prompt로 이미지 분류나 검색을 할 수 있다.
- 고정된 class label만 쓰는 지도학습보다 유연한 zero-shot 전이가 가능하다.

## 로봇 관점에서 보기

로봇에게 "빨간 컵", "잡을 수 있는 도구", "가장 작은 물체" 같은 언어 지시를 연결하려면 시각 정보와 언어 정보가 같은 기준으로 비교되어야 한다. CLIP류 모델은 이 연결의 기본 형태를 제공한다.

## 확인 질문

- contrastive learning이 무엇인가?
- zero-shot classification은 어떻게 가능한가?
- CLIP은 물체의 위치를 직접 주는가, 아니면 이미지-텍스트 유사도를 주는가?
- 실제 로봇 제어에 쓰려면 CLIP 출력 뒤에 무엇이 더 필요한가?
