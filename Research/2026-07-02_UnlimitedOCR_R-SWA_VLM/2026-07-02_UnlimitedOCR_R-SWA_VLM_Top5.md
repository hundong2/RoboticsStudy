# 2026-07-02 VLM 리서치 노트: Unlimited OCR / R-SWA 중심 Top 5

> 기준일: 2026년 7월 2일  
> 대표 이름: **Unlimited OCR / Reference Sliding Window Attention**  
> 주제: 장문 OCR 메모리 효율, 네이티브 3D VLM, 인식 중심 PRM, 도구 가이드 추론, 이벤트 센서 융합

## 먼저 보는 결론

이번 묶음의 대표 논문은 **Unlimited OCR Works**로 선정했다. 이유는 명확하다. VLM/MLLM이 실제 서비스로 들어가면 가장 먼저 부딪히는 문제는 "정확히 보느냐"만이 아니라 **긴 입력과 긴 출력을 예측 가능한 메모리로 처리할 수 있느냐**다. Unlimited OCR의 R-SWA는 이 문제를 attention/cache 구조 차원에서 다룬다.

동시에 VLM3, Perceval, Tool-Guided VLMs, RE-VLM은 서로 다른 방향에서 같은 질문을 건드린다.

- VLM3: 2D VLM이 3D 공간을 배울 수 있는가
- Perceval: VLM의 시각적 환각을 token/span 단위로 제어할 수 있는가
- Tool-Guided VLMs: 모델 학습 없이 시각 검증 도구로 오독을 줄일 수 있는가
- RE-VLM: RGB가 깨지는 환경에서 이벤트 센서로 장면 이해를 보강할 수 있는가

## 검증 메모

사용자가 제공한 링크 중 **Unlimited OCR의 Hugging Face 번호 `2606.06941`은 확인 가능한 대표 논문 번호와 맞지 않는다.** 실제 논문은 arXiv/Hugging Face 기준 **`2606.23050` Unlimited OCR Works**로 확인된다. 문서에는 확인된 번호를 기준으로 정리했다.

또한 "Hugging Face Trending 최상위권", "GitHub 스타 폭발", "학계 장악" 같은 표현은 실시간성과 해석이 강한 표현이므로, 본문에서는 기술적 의미 중심으로 완화했다. GitHub에서 확인되는 수치도 시간에 따라 바뀐다.

## Top 5 요약 표

| 순위 | 논문 | 확인된 출처 | 핵심 기술 | 실무 의미 |
|---:|---|---|---|---|
| 1 | Unlimited OCR Works | arXiv 2606.23050, Baidu GitHub | Reference Sliding Window Attention | 긴 문서 OCR의 KV cache 증가 억제 |
| 2 | VLM3 | arXiv 2605.30561, Meta | Focal length unification, text pixel reference | 2D VLM을 3D 공간 이해로 확장 |
| 3 | Perceval / Perception-centric PRM | arXiv 2604.24583, CVPR 2026 | token-level perceptual error grounding | VLM 환각을 추론 과정에서 교정 |
| 4 | Tool-Guided VLMs on Visual Illusions | arXiv 2603.29428, CVPRW 2026 | crop, line drawing, comparison, registry | 학습 없이 도구 호출로 시각 오독 완화 |
| 5 | RE-VLM | arXiv 2605.19329, CVPR 2026 | RGB-event dual stream | 저조도·HDR·고속 모션에서 강건성 향상 |

## 1. Unlimited OCR Works

**기관:** Baidu 계열 연구진  
**공개일:** 2026년 6월 22일  
**대표 기술:** Reference Sliding Window Attention, R-SWA

### 문제

LLM 디코더 기반 OCR은 문서를 텍스트로 생성하면서 이전 출력 토큰의 `KV cache`를 계속 쌓는다. 짧은 영수증은 괜찮지만, 수십 페이지 문서나 긴 리포트를 한 번에 처리하면 출력 길이 `T`에 비례해 메모리와 attention 비용이 커진다.

표준 attention의 단순화된 cache 크기는 다음처럼 볼 수 있다.

```text
Full attention cache ~= reference_tokens + generated_tokens
```

문서 OCR에서는 `reference_tokens`가 시각 토큰이고, `generated_tokens`가 생성 중인 텍스트다. 문제는 문서가 길수록 생성 텍스트가 길어져 cache가 계속 증가한다는 점이다.

### 핵심 아이디어

R-SWA는 attention 대상을 두 구간으로 나눈다.

- **Reference memory:** 문서 이미지에서 온 고정 시각 토큰은 계속 참조한다.
- **Working memory:** 이전 출력 텍스트는 최근 `n`개 토큰만 본다.

즉, 디코더가 원본 문서 이미지는 계속 볼 수 있게 하되, 자신이 이미 길게 생성한 텍스트 전체를 매번 기억하지 않게 만든다.

```text
R-SWA cache ~= reference_tokens + sliding_window_size
```

이 구조는 사람이 긴 문서를 베껴 쓸 때와 닮았다. 원문은 계속 보고, 방금 쓴 몇 줄만 작업 기억에 둔다.

### 배울 점

긴 컨텍스트를 지원하는 것과 긴 출력을 저렴하게 생성하는 것은 다르다. 실무 문서 파싱에서는 `32K context`라는 숫자보다 **출력 길이가 늘어도 메모리와 latency가 예측 가능한가**가 더 중요하다.

## 2. VLM3: Vision Language Models Are Native 3D Learners

**기관:** Meta, Princeton University 계열 저자  
**공개일:** 2026년 5월 28일  
**대표 기술:** focal length unification, text-based pixel reference, data mixture

VLM3는 표준 VLM이 복잡한 3D 전용 구조 없이도 3D 태스크를 배울 수 있다고 주장한다. 핵심은 3D를 별도 모듈로 분리하기보다, 깊이 추정·카메라 포즈·픽셀 대응 같은 문제를 VLM이 이해할 수 있는 텍스트 기반 과제로 바꾸는 것이다.

중요한 설계는 다음 세 가지다.

- **초점 거리 통일:** 카메라 파라미터 차이 때문에 깊이 스케일이 흔들리는 문제를 줄인다.
- **텍스트 기반 픽셀 참조:** 좌표나 픽셀 위치를 모델이 언어적으로 참조할 수 있게 한다.
- **데이터 혼합과 스케일링:** 한 가지 3D 태스크가 아니라 여러 3D 문제를 함께 학습한다.

로보틱스에서는 장면 설명보다 더 중요한 것이 공간 관계다. "컵이 있다"보다 "컵이 로봇 팔 기준 어느 방향, 어느 거리, 어느 표면 위에 있는가"가 필요하다. VLM3는 VLM을 그런 공간 추론 쪽으로 밀어붙이는 연구다.

## 3. Improving VLMs with Perception-centric Process Reward Models

**학회/출처:** CVPR 2026, arXiv 2604.24583  
**대표 이름:** Perceval  
**대표 기술:** token-level error grounding, process reward model

기존 reward는 최종 답변 전체를 평가하는 경우가 많다. 하지만 VLM의 오류는 보통 답변 전체가 갑자기 틀리는 식이 아니라, 중간에 이미지에 없는 시각적 주장을 끼워 넣으면서 시작된다.

Perceval은 모델 답변에서 이미지 관련 claim을 뽑고, 각 claim을 이미지 증거와 비교해 perceptual error가 있는 span을 찾는다. 그 결과를 학습에서는 token-level penalty로 쓰고, 추론 시점에는 잘못된 부분을 잘라내거나 재생성하도록 유도한다.

핵심은 "틀렸다"가 아니라 **어디서부터 시각 근거와 어긋났는가**를 찾는 것이다.

## 4. Seeing the Evidence, Missing the Answer: Tool-Guided VLMs

**학회/출처:** CVPR 2026 Workshops, DataCV  
**대표 기술:** tool-guided inference, immutable image registry, illusion routing

이 논문은 시각적 착시와 트릭 이미지에서 VLM이 가진 확증 편향을 다룬다. 모델은 이미지를 픽셀 단위로 볼 수 있는데도, 상위 의미나 학습된 패턴에 끌려 "그럴싸한 답"을 내는 경우가 있다.

해결책은 모델을 새로 학습시키는 것이 아니라, VLM에게 작은 도구 상자를 주는 것이다.

- 선 긋기
- 영역 자르기
- 나란히 비교하기
- 채널 분리하기
- 도구 결과를 immutable registry에 저장하기

이 방식의 장점은 간단하다. 모델이 눈으로 한 번 보고 답하는 대신, 증거 이미지를 만들고 그 증거를 다시 참조하면서 추론한다. 실제 제품에서는 OCR crop, GUI element zoom, 차트 축 확대, 이미지 비교 같은 기능으로 바로 이어질 수 있다.

## 5. RE-VLM: Event-Augmented Vision-Language Model

**학회/출처:** CVPR 2026, arXiv 2605.19329  
**대표 기술:** RGB-event dual stream, progressive training, graph-driven caption/QA synthesis

RE-VLM은 RGB 카메라가 약한 환경을 이벤트 카메라로 보완한다. 저조도, HDR, 빠른 움직임에서는 RGB 프레임이 흐려지거나 포화된다. 이벤트 카메라는 픽셀 밝기 변화 이벤트를 비동기적으로 기록하므로, 빠른 모션과 넓은 다이내믹 레인지에서 유리한 정보를 제공한다.

논문은 RGB encoder와 event encoder를 병렬로 두고, 두 센서 표현을 언어 공간에 정렬한다. 또한 RGB-Event-Text 데이터 부족을 scene graph 기반 합성 caption/QA로 보완한다.

실세계 로봇 비전에서 이 연구가 중요한 이유는 분명하다. 좋은 reasoning은 좋은 observation 위에서만 가능하다.

## 이번 주 기술 흐름

### 1. 메모리 효율이 성능만큼 중요해졌다

Unlimited OCR의 핵심은 "더 큰 모델"이 아니라 "더 오래 생성해도 무너지지 않는 attention 구조"다. 문서 AI, 장문 OCR, 멀티페이지 RAG에서는 이 방향이 특히 중요하다.

### 2. VLM은 픽셀을 더 구조적으로 확인해야 한다

Perceval과 Tool-Guided VLMs는 모두 VLM이 language prior에 끌려가는 문제를 다룬다. 하나는 reward model로, 다른 하나는 도구 호출과 evidence registry로 해결한다.

### 3. 3D와 센서 융합은 로보틱스 연결점이다

VLM3는 2D VLM에서 3D 공간 이해로, RE-VLM은 RGB에서 이벤트 센서 융합으로 확장한다. 둘 다 로봇이 실제 환경에서 안정적으로 움직이기 위한 기반 기술이다.

## 학습용 구현 파일

이 폴더에는 다음 예제가 포함되어 있다.

- `rswa_memory_demo.py`: Full attention과 R-SWA의 cache/token 비용 증가를 비교한다.
- `tool_guided_registry_demo.py`: Tool-Guided VLM 논문의 image registry 개념을 작은 숫자 이미지로 재현한다.
- `LEARNING_GUIDE.md`: 실행 순서, 관찰 포인트, 확장 과제를 정리했다.

## 참고 링크

- Unlimited OCR Works: https://arxiv.org/abs/2606.23050
- Unlimited OCR GitHub: https://github.com/baidu/Unlimited-OCR
- Hugging Face Papers - Unlimited OCR Works: https://huggingface.co/papers/2606.23050
- VLM3: https://arxiv.org/abs/2605.30561
- Hugging Face Papers - VLM3: https://huggingface.co/papers/2605.30561
- Perceval / Perception-centric PRM: https://arxiv.org/abs/2604.24583
- Tool-Guided VLMs on Visual Illusions: https://arxiv.org/abs/2603.29428
- CVPRW Open Access - Tool-Guided VLMs: https://openaccess.thecvf.com/content/CVPR2026W/DataCV/html/Wang_Seeing_the_Evidence_Missing_the_Answer_Tool-Guided_Vision-Language_Models_on_CVPRW_2026_paper.html
- RE-VLM: https://arxiv.org/abs/2605.19329
- CVPR Open Access - RE-VLM: https://openaccess.thecvf.com/content/CVPR2026/html/Liu_RE-VLM_Event-Augmented_Vision-Language_Model_for_Scene_Understanding_CVPR_2026_paper.html
- RE-VLM GitHub: https://github.com/bupt-ai-cz/RE-VLM
