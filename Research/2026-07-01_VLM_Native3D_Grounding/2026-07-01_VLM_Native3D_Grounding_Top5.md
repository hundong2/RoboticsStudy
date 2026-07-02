# 2026-07-01 VLM 최신 리서치 노트: Native 3D, Grounding, Perception Reward

> 기준일: 2026년 7월 1일 수요일  
> 대표 이름: **VLM3 + LocateAnything**  
> 범위: Hugging Face Papers, arXiv, CVPR 2026 Open Access에서 확인 가능한 VLM 관련 최신 논문 중심 정리

## 핵심 요약

이번 흐름은 기존 VLM에 비전 어댑터를 붙이는 수준을 넘어, **표준 VLM 구조 안에서 3D 공간을 학습하는 방법**, **좌표를 빠르고 안정적으로 예측하는 시각적 그라운딩**, **환각 문장을 과정 단위로 잡아내는 perception-centric reward**, **디퓨전 모델 지식을 증류하는 비용 절감형 학습**, **RGB 한계를 보완하는 이벤트 센서 융합**으로 압축된다.

다만 원문에 포함된 일부 표현은 검증 범위를 나누어 봐야 한다. 예를 들어 VLV Auto-Encoder는 2026년 6월 신규 논문이 아니라 arXiv 기준 2025년 7월 공개 논문이며, 최근 VLM 효율 학습 맥락에서 다시 읽을 가치가 있는 항목으로 분류하는 편이 정확하다.

## Top 5 빠른 표

| 순위 | 논문 | 확인된 출처 | 핵심 질문 |
|---:|---|---|---|
| 1 | VLM3: Vision Language Models Are Native 3D Learners | arXiv 2605.30561, Meta/Princeton | 표준 VLM이 복잡한 3D 전용 구조 없이 깊이·포즈·대응점을 배울 수 있는가 |
| 2 | LocateAnything | arXiv 2605.27365, NVIDIA | 좌표 토큰을 순차 생성하지 않고 박스 단위로 병렬 예측할 수 있는가 |
| 3 | Improving VLMs with Perception-centric PRMs | arXiv 2604.24583, CVPR 2026 | VLM 환각을 답변 결과가 아니라 추론 과정 중간에서 잡을 수 있는가 |
| 4 | Vision-Language-Vision Auto-Encoder | arXiv 2507.07104 | 대규모 이미지-텍스트 쌍 없이 디퓨전 모델 지식을 VLM에 증류할 수 있는가 |
| 5 | RE-VLM | arXiv 2605.19329, CVPR 2026 | 저조도·HDR·고속 모션에서 RGB 한계를 이벤트 스트림으로 보완할 수 있는가 |

## 1. VLM3: Vision Language Models Are Native 3D Learners

**발표:** 2026년 5월 28일 arXiv  
**기관:** Meta, Princeton University 계열 저자  
**핵심 키워드:** focal length unification, text-based pixel reference, 3D learning with standard VLMs

VLM3의 핵심 주장은 명확하다. 3D 이해를 위해 반드시 별도 3D 모듈, 복잡한 regression loss, 대규모 3D augmentation을 붙여야 하는 것은 아니라는 것이다. 논문은 표준 VLM도 적절한 데이터 혼합과 표현 설계를 통해 깊이 추정, 픽셀 대응, 카메라 포즈 추정, 객체 수준 3D 이해를 학습할 수 있다고 주장한다.

중요한 설계는 세 가지다.

- **Focal Length Unification:** 카메라 초점 거리 차이 때문에 같은 장면도 깊이 스케일이 달라지는 문제를 줄인다.
- **Text-based Pixel Reference:** 픽셀 위치나 영역을 텍스트 표현으로 참조하게 만들어 VLM의 언어 인터페이스를 유지한다.
- **Data Mixture and Scaling:** 3D 전용 모델처럼 구조를 복잡하게 만들기보다, 여러 3D 태스크를 텍스트 기반 학습 데이터로 섞어 학습한다.

로보틱스 관점에서는 이 논문이 중요하다. 로봇은 물체 이름을 아는 것보다 **어디에 있고, 얼마나 떨어져 있고, 어떤 방향으로 움직일 수 있는지**를 알아야 한다. VLM3는 2D 기반 VLM을 3D 공간 이해 쪽으로 확장하는 단순한 레시피를 제시한다는 점에서 실용성이 있다.

## 2. LocateAnything: Parallel Box Decoding

**발표:** 2026년 5월 26일 arXiv  
**기관:** NVIDIA  
**핵심 키워드:** visual grounding, detection, Parallel Box Decoding, PBD

기존 VLM 기반 grounding은 박스를 `(x1, y1, x2, y2)` 같은 좌표 토큰 열로 만들어 순차 생성하는 경우가 많다. 이 방식은 두 가지 문제가 있다.

- 한 좌표를 만든 뒤 다음 좌표를 만들어야 하므로 느리다.
- 박스는 네 좌표가 함께 의미를 갖는데, 토큰별 생성은 박스의 기하 구조를 깨뜨리기 쉽다.

LocateAnything은 박스나 포인트를 **원자적 geometric unit**으로 보고 한 번에 예측하는 Parallel Box Decoding을 제안한다. 이 접근은 DETR류 set prediction의 장점과 생성형 VLM 인터페이스를 연결하는 방향으로 이해할 수 있다.

실무적으로는 GUI 자동화, 로봇 물체 집기, 문서 영역 탐지, 화면 요소 클릭 같은 작업에 직접 닿는다. "빨간 버튼을 찾아라", "나사 머리를 집어라", "표의 오른쪽 아래 셀을 읽어라" 같은 명령은 결국 언어를 좌표로 바꾸는 문제이기 때문이다.

이 폴더에는 이 아이디어를 교육용으로 단순화한 구현 예제 `parallel_box_decoding_demo.py`를 함께 넣었다.

## 3. Improving Vision-Language Models with Perception-centric Process Reward Models

**발표:** 2026년 4월 27일 arXiv, CVPR 2026 Open Access 확인  
**핵심 키워드:** Perceval, PRM, token-level error grounding, hallucination reduction

언어 모델의 RLVR/PRM 흐름은 "최종 답이 맞는가"뿐 아니라 "풀이 과정이 올바른가"를 보상으로 다룬다. 이 논문은 같은 관점을 VLM에 적용한다. VLM의 환각은 단순한 문장 오류가 아니라, **이미지에 없는 시각적 주장**을 만들어내는 문제다.

논문에서 제안하는 Perceval은 모델 응답에서 이미지 관련 claim을 추출하고, 각 claim이 실제 시각 증거와 맞는지 비교한다. 틀린 claim이 있는 span을 찾아 token-level penalty를 줄 수 있고, 추론 시점에는 잘못된 부분을 잘라내거나 재생성하도록 유도할 수 있다.

학습 관점에서 중요한 점은 sequence-level reward보다 더 촘촘한 신호를 준다는 것이다. "답 전체가 틀렸다"가 아니라 "이 문장의 이 시각 주장부터 틀렸다"를 알려준다.

## 4. Vision-Language-Vision Auto-Encoder

**발표:** 2025년 7월 9일 arXiv  
**핵심 키워드:** diffusion distillation, VLV auto-encoder, low-cost VLM training

이 항목은 원문처럼 2026년 6월 신규 논문으로 보기보다는, 최근 VLM 효율 학습 흐름에서 다시 볼 만한 선행 연구로 정리하는 것이 맞다.

VLV Auto-Encoder는 고품질 VLM captioner를 만들기 위해 수십억 이미지-텍스트 pair를 직접 모으는 대신, 이미 학습된 텍스트-이미지 디퓨전 모델의 지식을 활용한다. 큰 틀은 다음과 같다.

- vision encoder가 이미지 표현을 만든다.
- frozen T2I diffusion decoder가 언어 중심 representation에 압력을 건다.
- 이후 LLM decoder가 중간 표현을 풍부한 설명문으로 바꾼다.

핵심은 **paired image-text data 의존도를 줄이고 pretrained diffusion model의 시각-언어 정렬 지식을 증류한다**는 점이다. 로봇 데이터처럼 고품질 라벨을 대량으로 만들기 어려운 영역에서는 이런 비용 절감형 증류 전략이 중요해진다.

## 5. RE-VLM: Event-Augmented VLM

**발표:** 2026년 5월 19일 arXiv, CVPR 2026 Open Access 확인  
**핵심 키워드:** RGB-event fusion, dual-stream encoder, robust scene understanding

RE-VLM은 RGB 이미지가 취약한 환경을 정면으로 다룬다. 저조도, HDR, 빠른 모션에서는 일반 카메라 프레임이 흐려지거나 포화되기 쉽다. 이벤트 카메라는 픽셀 밝기 변화 이벤트를 비동기적으로 기록하므로 빠른 움직임과 극단적 조명에서 보완 정보를 제공할 수 있다.

논문은 RGB encoder와 event encoder를 병렬로 두고, progressive training으로 서로 다른 시각 특징을 언어 공간에 정렬한다. 또한 RGB-Event-Text 데이터 부족을 해결하기 위해 scene graph 기반으로 caption과 QA를 합성하는 파이프라인을 제안한다.

로봇 비전에서는 센서 융합이 핵심이다. 실험실 이미지가 아니라 실제 공장, 야외, 이동 플랫폼을 다루면 입력은 항상 깨끗하지 않다. RE-VLM은 "모델을 더 똑똑하게 만들기 전에 관측 채널을 더 강건하게 만들자"는 방향을 보여준다.

## 구현 예제: Parallel Box Decoding 데모

아래 파일을 실행하면 순차 좌표 생성 방식과 박스 단위 병렬 예측 방식의 차이를 간단히 볼 수 있다.

```powershell
python Research\2026-07-01_VLM_Native3D_Grounding\parallel_box_decoding_demo.py
```

이 코드는 실제 LocateAnything 모델 구현이 아니다. 논문 아이디어 중 **좌표를 토큰별로 분해하면 느리고 박스 일관성이 깨질 수 있으며, 박스 단위 예측은 병렬화와 geometry validation에 유리하다**는 부분을 작은 예제로 구현한 것이다.

## 기술 트렌드 3줄 정리

1. **2D VLM에서 3D 공간 지능으로:** VLM3는 표준 VLM 구조를 유지하면서도 깊이·포즈·픽셀 대응을 학습할 수 있다는 방향을 제시한다.
2. **좌표와 환각을 더 세밀하게 다루기:** LocateAnything은 좌표 디코딩 병목을 줄이고, Perceval은 시각적 환각을 token/span 단위로 잡아낸다.
3. **학습 비용과 실환경 강건성의 동시 최적화:** VLV Auto-Encoder는 증류로 학습 비용을 낮추고, RE-VLM은 이벤트 센서로 현실 입력의 품질 문제를 보완한다.

## 검증 메모

- Hugging Face Trending의 실시간 순위와 upvote 수는 시간에 따라 바뀌므로 본문에서는 확정 순위보다 기술 흐름 중심으로 정리했다.
- VLM3, LocateAnything, Perceval, RE-VLM은 2026년 arXiv 또는 CVPR 2026 Open Access에서 확인된다.
- VLV Auto-Encoder는 arXiv 기준 2025년 7월 논문이다. 따라서 "2026년 6월 말 신규 공개"로 쓰는 것은 부정확하다.

## 참고 링크

- VLM3: https://arxiv.org/abs/2605.30561
- VLM3 GitHub: https://github.com/facebookresearch/VLM3
- LocateAnything: https://arxiv.org/abs/2605.27365
- NVIDIA LocateAnything project: https://research.nvidia.com/labs/lpr/locate-anything/
- Hugging Face nvidia/LocateAnything-3B: https://huggingface.co/nvidia/LocateAnything-3B
- Perceval / Perception-centric PRM: https://arxiv.org/abs/2604.24583
- CVPR 2026 Perceval page: https://openaccess.thecvf.com/content/CVPR2026/html/Min_Improving_Vision-language_Models_with_Perception-centric_Process_Reward_Models_CVPR_2026_paper.html
- Vision-Language-Vision Auto-Encoder: https://arxiv.org/abs/2507.07104
- VLV project page: https://lambert-x.github.io/Vision-Language-Vision/
- RE-VLM: https://arxiv.org/abs/2605.19329
- CVPR 2026 RE-VLM PDF: https://openaccess.thecvf.com/content/CVPR2026/papers/Liu_RE-VLM_Event-Augmented_Vision-Language_Model_for_Scene_Understanding_CVPR_2026_paper.pdf
