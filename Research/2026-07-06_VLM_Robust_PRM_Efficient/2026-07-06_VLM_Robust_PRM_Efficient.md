# 2026-07-06 VLM 리서치 노트: 센서 강건성, 문서 파싱, PRM, 효율 모델

> 기준일: 2026년 7월 6일  
> 대표 이름: **RE-VLM**  
> 핵심 질문: VLM은 깨끗한 RGB 이미지가 아닐 때도 믿을 수 있는가?

## 오늘의 결론

7월 6일 흐름은 **실세계 입력의 불완전함**과 **모델 내부의 넘겨짚기**를 동시에 다룬다. RE-VLM은 센서 융합으로 관측을 강화하고, Perceval은 token-level 보상으로 환각을 줄이며, PaddleOCR-VL-1.6과 Eve는 실무 배포 효율을 겨냥한다.

## 왜 필요한가

VLM이 데모에서는 잘해도 현장에서는 다음 문제가 생긴다.

- 저조도, HDR, 빠른 움직임에서 RGB가 망가진다.
- 문서의 표/수식/차트에서 환각이 난다.
- 답변 전체 점수만으로는 어느 토큰부터 틀렸는지 알 수 없다.
- 작은 모델에 비전을 붙이면 언어 능력이 떨어질 수 있다.

## Top 5 정리

| 순위 | 논문 | 확인된 메타데이터 | 핵심 |
|---:|---|---|---|
| 1 | RE-VLM | arXiv 2605.19329, CVPR 2026 | RGB + event stream dual-stream VLM |
| 2 | PaddleOCR-VL-1.6 | arXiv 2606.03264 | under-optimized region refinement |
| 3 | Perceval / Perception-centric PRM | arXiv 2604.24583, CVPR 2026 | token-level perceptual error grounding |
| 4 | VLV Auto-Encoder | arXiv 2507.07104 | diffusion distillation |
| 5 | Eve | arXiv 2501.04322, AAAI 2025 | elastic visual experts로 언어 능력 보존 |

## 1. RE-VLM

RE-VLM은 RGB와 event camera stream을 함께 사용한다. 이벤트 카메라는 픽셀 밝기 변화만 비동기적으로 기록하므로 저조도, 고속 모션, HDR 조건에서 RGB보다 강한 단서를 줄 수 있다.

### 왜 써야 하나

로봇과 자율주행은 스튜디오 조명이 아니라 어두운 창고, 역광, 빠른 회전, 흔들리는 카메라에서 동작한다. RGB-only VLM은 입력 자체가 망가지면 reasoning이 좋아도 틀린다.

## 2. PaddleOCR-VL-1.6

문서 파싱에서 전체 데이터만 늘리는 대신, 이전 모델이 불안정한 region을 찾아 targeted enhancement와 progressive post-training을 적용한다.

### 왜 써야 하나

문서 AI는 평균 점수보다 "어디서 계속 틀리는가"가 중요하다. 표, 수식, 차트, 다국어 영역 같은 약점을 찾아 보강해야 제품 품질이 오른다.

## 3. Perception-centric PRM

Perceval은 VLM 응답에서 이미지 관련 claim을 추출하고, claim이 실제 visual evidence와 맞는지 비교해 hallucinated span을 찾는다.

### 왜 써야 하나

답변 전체가 틀렸다는 신호는 너무 거칠다. "이 문장의 이 구간이 이미지와 어긋났다"는 token-level 신호가 있어야 학습과 추론 교정이 가능하다.

## 4. VLV Auto-Encoder

기존 T2I diffusion model의 decoder를 활용해 visual-language alignment 지식을 증류한다.

### 왜 써야 하나

고품질 image-text pair를 새로 모으는 비용이 크다. 이미 학습된 diffusion model의 지식을 teacher로 쓰면 데이터 비용을 낮출 수 있다.

## 5. Eve

Eve는 Elastic Visual Experts로 작은 VLM에서 언어 능력 손상 없이 multimodal ability를 강화하려는 구조다.

### 왜 써야 하나

엣지 장비에서는 7B~70B VLM을 항상 돌릴 수 없다. 작은 모델에서 언어 능력과 비전 능력을 모두 보존하는 설계가 필요하다.

## 실습 연결

7월 10일 통합 실습 프로젝트에서 다음을 다룬다.

- RE-VLM 직관: RGB confidence와 event evidence 융합
- Perceval 직관: claim-level hallucination verifier
- Eve 직관: cost-aware expert routing

## 참고 링크

- RE-VLM: https://arxiv.org/abs/2605.19329
- PaddleOCR-VL-1.6: https://arxiv.org/abs/2606.03264
- Perceval: https://arxiv.org/abs/2604.24583
- VLV Auto-Encoder: https://arxiv.org/abs/2507.07104
- Eve: https://arxiv.org/abs/2501.04322
