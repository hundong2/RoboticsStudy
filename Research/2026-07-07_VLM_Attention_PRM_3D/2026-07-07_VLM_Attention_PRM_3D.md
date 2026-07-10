# 2026-07-07 VLM 리서치 노트: R-SWA, Perception PRM, 3D 공간 지능

> 기준일: 2026년 7월 7일  
> 대표 이름: **Unlimited OCR + Perceval + HiSpatial**  
> 핵심 질문: VLM을 길게, 정확하게, 공간적으로 만들려면 무엇이 필요한가?

## 오늘의 결론

이날 흐름은 세 가지 병목을 겨냥한다.

- 긴 문서 출력의 `KV cache` 병목: Unlimited OCR
- 시각적 환각의 fine-grained correction: Perceval
- 2D 관측에서 3D 공간 이해로의 확장: HiSpatial

## 왜 필요한가

VLM이 실제 시스템에 들어가면 긴 문서, 고해상도 차트, 3D 공간 명령을 동시에 다뤄야 한다. 이때 단순히 모델 크기만 키우면 메모리 비용, 환각, 공간 오해가 해결되지 않는다.

## Top 5 정리

| 순위 | 논문 | 확인된 메타데이터 | 핵심 |
|---:|---|---|---|
| 1 | Unlimited OCR Works | arXiv 2606.23050 | Reference Sliding Window Attention |
| 2 | Perceval | arXiv 2604.24583 | token-level PRM |
| 3 | HiSpatial | arXiv 2603.25411, CVPR 2026 | 4단계 3D spatial hierarchy |
| 4 | RE-VLM | arXiv 2605.19329 | RGB-event fusion |
| 5 | PaddleOCR-VL-1.6 | arXiv 2606.03264 | region-aware document post-training |

## 핵심 원리

### Unlimited OCR

긴 문서 OCR은 출력 토큰이 길다. full attention은 생성 토큰 전체를 계속 cache하므로 비용이 증가한다. R-SWA는 reference token은 유지하고 generated token은 sliding window로 제한한다.

```text
Full cache = reference + generated
R-SWA cache = reference + fixed_window
```

### Perceval

sequence-level reward는 coarse하다. Perceval은 이미지 관련 claim을 token/span 단위로 검증한다.

```text
response -> visual claims -> evidence check -> hallucinated spans -> penalty/regenerate
```

### HiSpatial

HiSpatial은 3D spatial understanding을 4단계로 분해한다.

```text
geometric perception -> object properties -> spatial relations -> abstract reasoning
```

### 왜 써야 하나

- R-SWA: 긴 문서 서빙 비용을 예측 가능하게 만든다.
- Perceval: 환각이 발생한 위치를 찾아 학습/추론 교정을 가능하게 한다.
- HiSpatial: 로봇/AR/자율주행에 필요한 공간 관계를 명시적으로 학습한다.

## 실습 연결

7월 10일 통합 실습 프로젝트:

- R-SWA cache simulator
- Perceval-style claim verifier
- HiSpatial-style 3D relation classifier

## 참고 링크

- Unlimited OCR Works: https://arxiv.org/abs/2606.23050
- Perceval: https://arxiv.org/abs/2604.24583
- HiSpatial: https://arxiv.org/abs/2603.25411
- RE-VLM: https://arxiv.org/abs/2605.19329
- PaddleOCR-VL-1.6: https://arxiv.org/abs/2606.03264
