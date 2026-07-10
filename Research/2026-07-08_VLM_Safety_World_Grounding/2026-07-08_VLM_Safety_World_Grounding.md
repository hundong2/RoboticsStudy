# 2026-07-08 VLM 리서치 노트: 비디오 안전성, 세계 모델, 환각 위치 특정

> 기준일: 2026년 7월 8일  
> 대표 이름: **HarmVideoBench + GAVEL**  
> 핵심 질문: VLM이 진짜로 보고 이해했는가, 아니면 텍스트처럼 맞힌 것인가?

## 오늘의 결론

7월 8일 흐름은 VLM의 "겉보기 지능"을 벗겨내는 평가셋과, 동적 세계를 상상하는 모델이 중심이다.

- HarmVideoBench: 유해 비디오 이해의 계층적 benchmark
- GAVEL: caption 오류 검증/설명/위치 특정
- Multiplayer Interactive World Models: 다중 에이전트 world model
- Risk-aware Driver Monitoring: 온디바이스 selective inference
- Unlimited OCR: 긴 문서 attention 비용 최적화

## 왜 필요한가

VLM이 영상이나 이미지 설명을 맞힌 것처럼 보여도 실제로는 shortcut을 쓸 수 있다. 안전 영역에서는 shortcut이 위험하다.

- 유해 영상: 표면 단서가 아니라 맥락과 인과를 이해해야 한다.
- 이미지 caption: 틀린 설명을 detect, explain, localize해야 한다.
- driver monitoring: 빠르면서도 위험한 false negative를 줄여야 한다.

## Top 5 정리

| 순위 | 논문 | 확인된 메타데이터 | 핵심 |
|---:|---|---|---|
| 1 | Unlimited OCR Works | arXiv 2606.23050 | R-SWA |
| 2 | HarmVideoBench | arXiv 2606.27187 | harmful video understanding benchmark + BCR |
| 3 | Multiplayer Interactive World Models | arXiv 2607.05352 | multiplayer action-conditioned world model |
| 4 | GAVEL | arXiv 2606.26923 | caption error verification/explanation/localization |
| 5 | Risk-Aware Driver Monitoring | arXiv 2606.26922 | selective inference + driver-state world modeling |

## HarmVideoBench

HarmVideoBench는 1,379개 영상과 4,137개 multiple-choice 질문으로 구성된 harmful-video benchmark다. 세 계층을 본다.

```text
Observable Evidence -> Clip-Internal Meaning -> Beyond-Clip Reasoning
```

BCR은 필요한 때에만 맥락을 검색하도록 reasoning boundary를 제한한다.

### 왜 써야 하나

비디오 안전성은 단순 flagging이 아니라 "왜 위험한지"를 설명할 수 있어야 한다. 특히 클립 내부와 클립 밖 맥락을 구분하는 것이 중요하다.

## GAVEL

GAVEL은 image-caption pair에서 오류를 검증하고, 오류를 설명하며, 시각 증거 위치를 찾는 benchmark다.

### 왜 써야 하나

환각을 줄이려면 "caption이 틀렸다"만으로 부족하다. 어떤 단어가 틀렸고, 이미지의 어느 영역이 증거인지 연결해야 한다.

## Multiplayer Interactive World Models

Rocket League 같은 빠른 다중 에이전트 환경에서 action stream을 조건으로 미래 frame을 예측한다.

### 왜 써야 하나

다중 에이전트 환경에서는 다른 agent를 단순한 배경으로 취급하면 안 된다. 누가 어떤 행동으로 장면 변화를 만들었는지 attribution이 필요하다.

## Risk-Aware Driver Monitoring

가벼운 RGB-physiology student가 빠르게 판단하고, gate가 위험할 때 abstain/slow branch를 선택한다.

### 왜 써야 하나

항상 큰 모델을 쓰면 latency가 높고, 항상 작은 모델을 쓰면 위험한 false negative가 생긴다. 선택적 추론은 비용과 안전을 동시에 다룬다.

## 참고 링크

- HarmVideoBench: https://arxiv.org/abs/2606.27187
- GAVEL: https://arxiv.org/abs/2606.26923
- Multiplayer Interactive World Models: https://arxiv.org/abs/2607.05352
- Risk-Aware Driver Monitoring: https://arxiv.org/abs/2606.26922
- Unlimited OCR Works: https://arxiv.org/abs/2606.23050
