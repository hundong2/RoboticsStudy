# 2026-07-03 VLM 리서치 노트: 효율 문서 파싱, 3D 공간 추론, 세계 모델 증류

> 기준일: 2026년 7월 3일 금요일  
> 대표 이름: **Unlimited OCR / R-SWA**  
> 핵심 질문: "왜 이 기술을 써야 하는가?"

## 결론부터

이번 묶음의 핵심은 **VLM을 더 크게 만드는 것보다, 실제로 쓸 수 있게 만드는 기술**이다. 문서 파싱은 메모리와 비용 때문에 막히고, 3D 공간 추론은 2D 이미지 설명만으로 부족하며, 동적 장면 이해는 추론 시점마다 세계 모델을 붙이면 너무 비싸다.

그래서 이번 흐름은 세 방향으로 정리된다.

- **기억을 줄인다:** Unlimited OCR의 R-SWA는 긴 출력에서 `KV cache`가 계속 커지는 문제를 줄인다.
- **모델을 작게 만든다:** SmolDocling은 256M급 compact VLM으로 문서를 구조화한다.
- **공간을 내부화한다:** SpatialStack과 World2VLM은 3D/동적 공간 정보를 VLM 내부 표현으로 넣는다.

## 왜 써야 하는가

VLM 연구를 볼 때 "성능이 높다"만 보면 실무 판단이 어렵다. 실제로는 다음 질문이 더 중요하다.

| 질문 | 관련 논문 | 왜 중요한가 |
|---|---|---|
| 긴 문서를 넣어도 GPU 메모리가 터지지 않는가 | Unlimited OCR | 문서 AI는 한두 페이지보다 수십 페이지 리포트가 진짜 문제다 |
| 작은 모델로도 문서 구조를 뽑을 수 있는가 | SmolDocling | 사내 서버, 엣지 장비, 비용 제한 환경에서는 70B VLM을 못 쓴다 |
| 로봇/AR이 3D 관계를 이해하는가 | SpatialStack | "왼쪽", "앞", "뒤", "가려짐"은 2D caption만으로 부족하다 |
| 디퓨전 모델 지식을 싸게 흡수할 수 있는가 | VLV Auto-Encoder | 이미지-텍스트 데이터를 대량으로 다시 모으는 비용이 너무 크다 |
| 움직이면 장면이 어떻게 바뀌는지 아는가 | World2VLM | 에이전트는 정적 인식보다 행동 결과 예측이 중요하다 |

## 검증 메모

사용자가 제공한 원문은 최신 트렌드 요약 형태라 일부 날짜와 표현을 보정했다.

- **Unlimited OCR Works**는 arXiv `2606.23050`으로 확인된다. Hugging Face paper page도 같은 번호를 사용한다.
- **SmolDocling**은 arXiv `2503.11576`이며 2025년 3월 공개, ICCV 2025 논문으로 확인된다. 2026년 신규 논문으로 쓰는 것은 부정확하다.
- **SpatialStack**은 arXiv `2603.27437`, CVPR 2026 논문으로 확인된다.
- **Vision-Language-Vision Auto-Encoder**는 arXiv `2507.07104`로 2025년 7월 공개 논문이다.
- **World2VLM**은 arXiv `2604.26934`로 확인된다.

## Top 5 요약

| 순위 | 논문 | 대표 기술 | 한 줄 판단 |
|---:|---|---|---|
| 1 | Unlimited OCR Works | Reference Sliding Window Attention | 긴 OCR 출력의 메모리 증가를 구조적으로 억제 |
| 2 | SmolDocling | 256M compact VLM, DocTags | 작은 모델로 end-to-end 문서 변환을 노림 |
| 3 | SpatialStack | layered geometry-language fusion | 3D 기하 정보를 LLM 계층에 단계적으로 주입 |
| 4 | Vision-Language-Vision Auto-Encoder | diffusion distillation | 디퓨전 모델의 시각-언어 지식을 증류 |
| 5 | World2VLM | world model imagination distillation | 세계 모델을 추론 도구가 아니라 학습 교사로 사용 |

## 1. Unlimited OCR Works

**출처:** Baidu, arXiv 2606.23050  
**핵심 기술:** Reference Sliding Window Attention, R-SWA  
**대표 선정 이유:** 오늘 묶음에서 실무 병목을 가장 직접적으로 찌른다.

### 무엇이 문제인가

LLM 디코더 기반 OCR은 텍스트를 길게 생성할수록 이전 토큰의 key/value를 `KV cache`에 계속 저장한다.

```text
Full attention cache ~= reference tokens + generated tokens
```

문서가 길어질수록 `generated tokens`가 늘어나므로 메모리와 latency가 증가한다. 이것은 긴 문서 OCR에서 치명적이다.

### R-SWA의 아이디어

Unlimited OCR은 "원문 문서"와 "최근 출력"을 분리한다.

```text
R-SWA cache ~= reference tokens + fixed sliding window
```

문서 이미지에서 온 reference token은 계속 참조하지만, 이미 오래전에 생성한 출력 토큰은 고정 길이 window 밖으로 밀어낸다. 사람도 책을 베껴 쓸 때 원문은 계속 보고, 방금 쓴 문장만 작업 기억에 둔다.

### 왜 써야 하나

- 긴 PDF, 리포트, 매뉴얼을 페이지 단위로 쪼개지 않고 처리하고 싶을 때 필요하다.
- 출력 길이에 따라 GPU 메모리가 계속 증가하면 서비스 비용 예측이 불가능하다.
- R-SWA는 정확도를 조금 높이는 기술이라기보다, **긴 문서 파싱을 운영 가능한 문제로 바꾸는 기술**이다.

## 2. SmolDocling

**출처:** arXiv 2503.11576, ICCV 2025  
**핵심 기술:** 256M parameter compact VLM, DocTags markup  
**정정:** 2026년 6월 신규 공개가 아니라 2025년 공개 논문이다.

### 무엇을 해결하나

문서 변환은 OCR만으로 끝나지 않는다. 실제로 필요한 것은 다음 구조다.

- 본문 텍스트
- 표
- 수식
- 코드 블록
- 차트
- 리스트
- 위치 정보
- 읽기 순서

SmolDocling은 이를 `DocTags`라는 구조적 markup으로 생성한다. 작은 VLM이 문서를 단순 텍스트가 아니라 **기계가 다시 처리할 수 있는 구조화 문서**로 바꾸는 것이 핵심이다.

### 왜 써야 하나

- 대형 VLM API에 의존하지 않고 문서 변환 파이프라인을 만들고 싶을 때 유리하다.
- 사내 문서, 제조 문서, 기술 매뉴얼처럼 외부 API로 보내기 어려운 데이터를 처리할 수 있다.
- 작은 모델이면 배포 비용, latency, GPU 요구량이 줄어든다.
- RAG에서 문서를 chunk로 쪼개기 전에 표/수식/위치 정보를 보존할 수 있다.

## 3. SpatialStack

**출처:** arXiv 2603.27437, CVPR 2026  
**핵심 기술:** layered geometry-language fusion

### 무엇을 해결하나

VLM은 이미지 설명은 잘하지만 3D 관계에는 약하다.

```text
"컵이 테이블 위에 있다"는 맞히지만,
"카메라가 오른쪽으로 움직이면 컵이 화면 어디로 이동하는가"는 어려워한다.
```

기존 방식은 vision encoder와 geometry encoder의 마지막 feature만 합치는 late fusion이 많았다. SpatialStack은 계층별 geometry feature를 LLM decoder layer에 점진적으로 주입한다.

### 왜 써야 하나

- 로봇, AR/VR, 자율주행은 2D 설명보다 3D 관계가 중요하다.
- local geometry와 global semantics를 동시에 보존해야 한다.
- late fusion은 깊은 layer 하나에서 정보를 합치므로 중간 수준 기하 정보를 잃기 쉽다.
- SpatialStack식 계층 융합은 "어디에 있는가"와 "무엇인가"를 같이 다루는 방향이다.

## 4. Vision-Language-Vision Auto-Encoder

**출처:** arXiv 2507.07104  
**핵심 기술:** diffusion model knowledge distillation  
**정정:** 2026년 6월 말 신규 논문이 아니라 2025년 7월 공개 논문이다.

### 무엇을 해결하나

고성능 VLM은 보통 대규모 이미지-텍스트 pair와 막대한 compute를 요구한다. VLV Auto-Encoder는 이미 잘 학습된 text-to-image diffusion model의 지식을 활용해 이 비용을 줄이려 한다.

핵심 구조는 다음과 같다.

```text
image -> vision encoder -> latent representation -> diffusion decoder / LLM decoder
```

디퓨전 모델이 가진 시각-언어 정렬 지식을 VLM 학습에 증류한다.

### 왜 써야 하나

- 고품질 image-text pair를 새로 모으는 비용을 줄이고 싶을 때 의미가 있다.
- 이미 존재하는 생성 모델의 시각 지식을 재사용할 수 있다.
- 데이터가 부족한 특수 도메인에서 pretrained generative model을 teacher로 활용할 수 있다.

## 5. World2VLM

**출처:** arXiv 2604.26934  
**핵심 기술:** world model imagination distillation

### 무엇을 해결하나

정적 VLM은 현재 이미지 설명에는 강하지만, 움직였을 때 장면이 어떻게 바뀔지 상상하는 데 약하다. World2VLM은 세계 모델을 추론 때마다 붙이는 대신, 학습 시점에 세계 모델이 만든 미래 view를 supervision으로 사용한다.

```text
초기 관찰 + 카메라 궤적
-> 세계 모델이 미래 view 생성
-> forward/action-to-outcome, inverse/outcome-to-action 학습
-> VLM 내부에 동적 공간 상상 능력 주입
```

### 왜 써야 하나

- 추론 시점마다 비싼 world model generation을 돌리지 않아도 된다.
- 에이전트가 행동 결과를 예측해야 하는 로봇/시뮬레이터/게임 환경에 적합하다.
- "움직이면 무엇이 보일까"와 "이 결과를 만들려면 어떻게 움직였을까"를 함께 학습한다.

## 실습 코드

같은 폴더에 [vlm_efficiency_spatial_demo.py](./vlm_efficiency_spatial_demo.py)를 추가했다.

실행:

```powershell
python Research\2026-07-03_VLM_Efficient_Doc_Spatial\vlm_efficiency_spatial_demo.py
```

이 코드는 실제 VLM을 실행하지 않는다. 대신 오늘 논문들의 핵심 원리를 작은 숫자 실험으로 확인한다.

- `R-SWA`: full cache와 fixed-window cache의 메모리 증가 비교
- `SmolDocling`: 문서 요소를 단순 OCR 텍스트가 아니라 DocTags 구조로 표현하는 이유
- `World2VLM`: world model을 추론 때 붙이는 방식과 학습 시 distillation하는 방식의 비용 차이

## 이번 주 기술 트렌드 3줄

1. **문서 AI는 긴 출력 비용 싸움이다.** Unlimited OCR은 decoder memory를 통제하고, SmolDocling은 모델 크기 자체를 줄인다.
2. **공간 지능은 2D caption을 넘어선다.** SpatialStack과 World2VLM은 3D geometry와 움직임에 따른 장면 변화를 VLM 내부로 넣는다.
3. **큰 teacher를 매번 쓰지 말고 내부화한다.** VLV Auto-Encoder와 World2VLM은 기존 강한 모델의 지식을 학습 시점에 흡수해 추론 비용을 줄인다.

## 참고 링크

- Unlimited OCR Works: https://arxiv.org/abs/2606.23050
- Hugging Face Papers - Unlimited OCR Works: https://huggingface.co/papers/2606.23050
- Baidu Unlimited-OCR GitHub: https://github.com/baidu/Unlimited-OCR
- SmolDocling: https://arxiv.org/abs/2503.11576
- Hugging Face Papers - SmolDocling: https://huggingface.co/papers/2503.11576
- SpatialStack: https://arxiv.org/abs/2603.27437
- SpatialStack project page: https://spatial-stack.github.io/
- Vision-Language-Vision Auto-Encoder: https://arxiv.org/abs/2507.07104
- World2VLM: https://arxiv.org/abs/2604.26934
