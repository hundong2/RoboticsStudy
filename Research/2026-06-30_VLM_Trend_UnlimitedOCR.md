# 2026-06-30 VLM 최신 리서치 노트: Unlimited OCR 중심 Top 5

> 기준일: 2026년 6월 30일 화요일  
> 대표 이름: **Unlimited OCR**  
> 주제: 장문 문서 파싱, 에이전트 월드 모델, 이벤트 센서 기반 강건 VLM, 데이터 큐레이션, 문서 파싱 후학습

## 한눈에 보는 결론

이번 주 VLM(Vision-Language Model) 흐름은 단순히 모델 크기를 키우는 방향보다 **긴 입력을 싸게 처리하는 구조**, **행동 결과를 미리 예측하는 에이전트용 세계 모델**, **나쁜 촬영 조건에서도 버티는 센서 융합**, **데이터 품질만으로 성능을 끌어올리는 방법론** 쪽으로 이동하고 있다.

대표 논문으로는 **Unlimited OCR Works**를 선정했다. 이유는 실무에서 가장 자주 부딪히는 병목인 긴 문서 OCR/파싱의 `KV cache` 메모리 증가 문제를 정면으로 다루며, 문서 자동화·RAG·로봇 매뉴얼 해석 같은 응용으로 바로 이어지기 때문이다.

## 빠른 순위

| 순위 | 논문 | 핵심 키워드 | 읽어야 하는 이유 |
|---:|---|---|---|
| 1 | Unlimited OCR Works | R-SWA, constant KV cache, long OCR | 긴 문서 파싱의 메모리 병목을 줄이는 구조적 접근 |
| 2 | Qwen-AgentWorld | language world model, agent simulation | 에이전트가 행동 전 환경 변화를 예측하는 방향 |
| 3 | RE-VLM | RGB-event fusion, robust perception | 저조도·고속 모션 등 실환경 인지 취약점 보완 |
| 4 | 20/20 Vision Language Models | data curation, benchmark lift | 아키텍처 변경 없이 데이터 정제로 성능을 높이는 사례 |
| 5 | PaddleOCR-VL-1.6 | document parsing, region refinement | 약한 문서 영역을 찾아 후학습으로 보강하는 실용 레시피 |

## 1. Unlimited OCR Works

**기관/저자:** Baidu 계열 연구진으로 표기된 arXiv 기술 보고서  
**공개일:** 2026년 6월 22일  
**대표 기술:** Reference Sliding Window Attention, R-SWA

### 무엇을 해결하나

LLM 디코더를 쓰는 OCR 모델은 출력 텍스트가 길어질수록 `KV cache`가 계속 쌓인다. 짧은 영수증이나 한두 장짜리 문서는 괜찮지만, 수십 페이지 문서·논문·매뉴얼을 한 번에 옮기려 하면 메모리 사용량과 디코딩 시간이 급격히 증가한다.

Unlimited OCR은 이 문제를 **사람의 작업 기억처럼 제한된 참조 범위만 유지하는 방식**으로 다룬다. DeepSeek OCR 계열의 고압축 비전 인코더를 기반으로, 디코더의 attention을 R-SWA로 바꾸어 디코딩 중 `KV cache`를 상수에 가깝게 유지하려는 구조다.

### 핵심 아이디어

- 긴 출력 전체를 모두 기억 대상으로 두지 않고, 참조 가능한 슬라이딩 윈도우를 둔다.
- 문서 이미지의 고압축 표현과 제한된 디코더 메모리를 결합한다.
- 표준 32K 최대 길이 조건에서 여러 페이지 문서를 한 번의 forward pass로 전사하는 방향을 제시한다.
- OCR뿐 아니라 ASR, 번역처럼 긴 시퀀스를 계속 생성하는 파싱 문제에도 확장 가능하다고 주장한다.

### 기술적으로 배울 점

`context length`를 늘리는 것과 `serving cost`를 낮추는 것은 다르다. 긴 컨텍스트를 지원해도 매 토큰마다 과거 전체를 참조하면 비용은 계속 증가한다. 이 논문은 VLM/OCR에서 중요한 질문을 던진다.

> 모델이 모든 과거 토큰을 기억해야만 정확한가, 아니면 작업에 필요한 참조 기억만 잘 설계하면 충분한가?

문서 자동화 시스템을 만든다면 이 관점이 중요하다. 긴 매뉴얼, CAD 도면 설명, 로봇 운용 로그, 검사 리포트를 처리할 때 병목은 정확도뿐 아니라 **페이지 수가 늘어도 예측 가능한 비용으로 처리되는가**이기 때문이다.

## 2. Qwen-AgentWorld: Language World Models for General Agents

**기관/저자:** Alibaba Qwen Team  
**공개일:** 2026년 6월 23일  
**대표 기술:** language world model, AgentWorldBench, agentic environment simulation

### 무엇을 해결하나

일반 에이전트는 웹, 모바일 GUI, 게임, 가상 환경, 로봇 제어처럼 서로 다른 환경에서 행동해야 한다. 문제는 행동을 실제로 실행한 뒤에야 결과를 알 수 있다는 점이다. 잘못 클릭하거나, 잘못 이동하거나, 잘못 조작하면 되돌리기 어렵다.

Qwen-AgentWorld는 언어 모델 기반 세계 모델을 통해 **현재 관찰과 행동을 입력받아 다음 상태를 예측하는 시뮬레이터**를 목표로 한다. 논문은 7개 도메인의 1천만 개 이상 환경 상호작용 trajectory를 활용해 모델을 학습했다고 설명한다.

### 핵심 아이디어

- 에이전트의 현재 관찰, 의도, 행동을 언어 중심 표현으로 정렬한다.
- 행동 후 환경이 어떻게 바뀔지 next-state prediction 형태로 예측한다.
- CPT, SFT, RL을 단계적으로 사용해 일반 세계 모델링 능력과 시뮬레이션 충실도를 높인다.
- AgentWorldBench라는 평가 벤치마크를 제안한다.

### 기술적으로 배울 점

VLM 에이전트의 다음 단계는 "화면을 보고 답하기"를 넘어 "행동하면 어떤 일이 생길지 상상하고 고르기"다. 로봇 분야로 옮기면 이는 motion planning의 고수준 버전과 닮았다.

- GUI 에이전트: 버튼을 누르기 전 화면 전환을 예측
- 자율주행/이동 로봇: 경로 선택 전 장면 변화를 예측
- 조작 로봇: 집기·밀기·회전 행동의 결과를 미리 평가

실제 제어기로 바로 쓰기보다는, 고수준 계획·시뮬레이션·데이터 생성에 먼저 연결될 가능성이 높다.

## 3. RE-VLM: Event-Augmented Vision-Language Model for Scene Understanding

**기관/저자:** Hanqing Liu 외  
**공개일:** 2026년 5월 19일  
**대표 기술:** RGB + event stream dual-stream VLM

### 무엇을 해결하나

일반 RGB 카메라는 저조도, 강한 역광, 빠른 움직임에서 쉽게 망가진다. 로봇이나 자율주행처럼 현실 환경에서 움직이는 시스템은 이런 조건을 피하기 어렵다. RE-VLM은 RGB 이미지와 이벤트 카메라 스트림을 함께 사용해 이 취약점을 줄이려는 연구다.

이벤트 카메라는 프레임 전체를 일정 주기로 찍는 대신, 픽셀 밝기 변화가 발생한 순간을 비동기적으로 기록한다. 그래서 빠른 움직임, 넓은 다이내믹 레인지, 어두운 장면에서 RGB보다 유리한 정보가 남을 수 있다.

### 핵심 아이디어

- RGB 인코더와 이벤트 인코더를 병렬로 두는 dual-stream 구조를 사용한다.
- 서로 다른 센서 표현을 언어 공간에 맞추기 위해 progressive training을 적용한다.
- RGB-Event-Text 데이터 부족을 보완하기 위해 scene graph 기반 합성 캡션과 QA를 생성한다.
- PEOD-Chat, RGBE-Chat 데이터셋을 통해 저조도와 일반 장면을 함께 평가한다.

### 기술적으로 배울 점

강건성은 프롬프트만으로 해결하기 어렵다. 입력 센서가 이미 망가졌다면 VLM은 그 위에서 추론할 수밖에 없다. RE-VLM은 **좋은 추론 이전에 좋은 관측이 필요하다**는 점을 보여준다.

로봇 비전 관점에서는 다음 질문으로 이어진다.

- RGB-D, thermal, event camera를 VLM에 어떻게 정렬할 것인가?
- 센서별 신뢰도가 상황마다 다를 때 attention이나 gating을 어떻게 설계할 것인가?
- 학습 데이터가 부족한 특수 센서에서 synthetic QA를 얼마나 믿을 수 있는가?

## 4. 20/20 Vision Language Models

**기관/저자:** Siddharth Joshi 외, DatalogyAI 관련 연구진  
**공개일:** 2026년 5월 12일  
**대표 기술:** data curation alone

### 무엇을 해결하나

많은 VLM 연구는 더 큰 모델, 더 긴 학습, 더 많은 데이터를 강조한다. 이 논문은 반대로 묻는다.

> 모델 구조, 학습 레시피, compute를 고정하고 데이터만 더 잘 고르면 얼마나 좋아지는가?

연구진은 MAmmoTH-VL single-image subset에 데이터 큐레이션 파이프라인을 적용했고, 20개 공개 VLM 벤치마크 평균에서 +11.7 percentage point 향상을 보고했다.

### 핵심 아이디어

- 중복, 저품질, 모호한 supervision, 편향된 샘플을 줄인다.
- grounding, VQA, OCR/document, chart, math, spatial/3D, counting 등 다양한 능력 축에서 평가한다.
- OOD 일반화, seed 안정성, 응답 FLOPs 관점에서도 이득을 분석한다.

### 기술적으로 배울 점

데이터 큐레이션은 "전처리"가 아니라 모델 성능을 바꾸는 핵심 기술이다. 특히 VLM은 이미지와 텍스트가 동시에 맞아야 하므로 노이즈의 형태가 복잡하다.

- 이미지에는 답이 있지만 캡션이 부정확한 경우
- 질문이 애매해 여러 답이 가능한 경우
- 차트/수식/문서 OCR처럼 작은 오류가 전체 reasoning을 무너뜨리는 경우
- 훈련 데이터가 벤치마크와 과하게 겹쳐 일반화처럼 보이는 경우

작은 모델을 실용적으로 쓰려면, 무작정 파라미터를 키우기보다 **좋은 데이터로 작은 모델을 효율적으로 훈련하는 전략**이 중요하다.

## 5. PaddleOCR-VL-1.6

**기관/저자:** PaddlePaddle/Baidu 계열 연구진  
**공개일:** 2026년 6월 2일  
**대표 기술:** under-optimized region refinement, progressive post-training

### 무엇을 해결하나

PaddleOCR-VL-1.6은 문서 파싱 모델을 더 크게 만드는 대신, 이전 모델이 약한 영역을 찾아 집중적으로 보강한다. 논문은 PaddleOCR-VL-1.5의 오류가 특정 "under-optimized regions"에 몰린다고 보고, 이를 지역 인식 데이터 최적화와 후학습으로 개선한다.

### 핵심 아이디어

- 모델이 불안정하게 동작하는 문서 영역을 식별한다.
- 데이터 커버리지가 부족하거나 supervision 품질이 낮은 영역을 타겟팅한다.
- curated data selection과 reinforcement learning을 포함한 progressive post-training을 적용한다.
- OmniDocBench v1.6에서 96.33% 점수를 보고한다.

### 기술적으로 배울 점

문서 파싱에서는 평균 성능보다 **어디서 틀리는가**가 중요하다. 실무 문서는 표, 각주, 수식, 도장, 워터마크, 작은 글자, 다국어가 섞인다. PaddleOCR-VL-1.6의 관점은 오류 분석을 학습 데이터 선택과 후학습으로 다시 연결한다는 점에서 실용적이다.

## 이번 주 핵심 기술 축

### 1. 긴 문서 처리 비용 줄이기

Unlimited OCR과 PaddleOCR-VL-1.6은 모두 문서 파싱 문제를 다루지만 초점이 다르다.

- Unlimited OCR: 긴 출력 시퀀스의 attention/KV cache 비용을 줄이는 구조
- PaddleOCR-VL-1.6: 모델이 약한 문서 영역을 찾아 데이터와 후학습으로 보강

실무에서는 두 접근이 함께 필요하다. 하나는 **비용을 예측 가능하게 만드는 기술**, 다른 하나는 **문서 유형별 오류를 줄이는 기술**이다.

### 2. 에이전트는 이제 결과를 예측해야 한다

Qwen-AgentWorld는 VLM/LLM 에이전트가 단순히 명령을 수행하는 단계에서 벗어나, 행동의 결과를 내부적으로 시뮬레이션하는 방향을 보여준다. 로봇이나 자동화 시스템에서는 이 기능이 실패 비용을 낮추는 데 중요하다.

### 3. 현실 센서는 깨끗하지 않다

RE-VLM은 실세계 VLM의 입력이 항상 선명한 RGB 이미지라는 가정을 깨뜨린다. 이벤트 카메라처럼 다른 센서를 결합하면, VLM이 저조도·고속 모션·HDR 환경에서 더 안정적으로 장면을 이해할 가능성이 있다.

### 4. 데이터 큐레이션은 성능 최적화 기법이다

20/20 Vision Language Models는 데이터 품질이 모델 구조만큼 중요하다는 점을 강하게 보여준다. 특히 작은 VLM을 현장 장비나 로봇에 올릴 때, 좋은 데이터 큐레이션은 compute 절약과 일반화 성능을 동시에 얻는 수단이 될 수 있다.

## 용어 정리

- **VLM:** 이미지와 텍스트를 함께 입력받아 설명, 질의응답, 추론을 수행하는 모델.
- **MLLM:** 멀티모달 입력을 다루는 대형 언어 모델. VLM보다 넓은 개념으로 오디오, 비디오, 센서 입력까지 포함할 수 있다.
- **KV cache:** Transformer 디코딩에서 이전 토큰의 key/value를 저장해 다음 토큰 생성을 빠르게 하는 캐시. 출력이 길수록 메모리 부담이 커진다.
- **Sliding Window Attention:** 전체 과거 토큰 대신 최근 또는 선택된 범위만 참조하도록 제한하는 attention 방식.
- **World Model:** 현재 상태와 행동을 바탕으로 다음 상태를 예측하는 모델. 에이전트 계획과 시뮬레이션에 쓰인다.
- **Event Camera:** 밝기 변화 이벤트를 비동기적으로 기록하는 센서. 빠른 움직임과 극단적 조명 조건에서 장점이 있다.
- **Data Curation:** 데이터 수집 후 중복, 노이즈, 모호한 라벨, 불량 샘플을 걸러 학습 품질을 높이는 과정.

## 로봇/비전 학습 관점에서의 적용 메모

- 로봇 매뉴얼, 검사 리포트, 실험 로그를 자동 파싱하려면 Unlimited OCR류의 긴 문서 처리 구조를 주목한다.
- GUI 자동화, 시뮬레이터 기반 로봇 학습, VLA 파이프라인을 공부한다면 Qwen-AgentWorld의 세계 모델 관점이 중요하다.
- 이동 로봇이나 고속 비전 시스템을 다룬다면 RE-VLM처럼 RGB 외 센서를 언어 모델과 정렬하는 방법을 살펴본다.
- 작은 모델을 현장 장비에 올려야 한다면 20/20 VLMs의 데이터 큐레이션 전략을 우선 검토한다.
- 문서 AI를 제품화한다면 PaddleOCR-VL-1.6처럼 평균 점수보다 실패 영역을 찾고 보강하는 루프가 필요하다.

## 검증 메모

이 문서는 사용자가 제공한 트렌드 요약을 바탕으로 하되, arXiv에서 확인 가능한 논문명, 공개일, 저자, 초록 정보를 기준으로 내용을 보완했다. Hugging Face Trending 순위, 세부 upvote 수, GitHub star 수, 특정 학회 수록 여부는 기준일의 실시간 페이지 상태에 따라 변할 수 있으므로 본문에서는 단정 표현을 줄였다.

## 참고 링크

- Unlimited OCR Works: https://arxiv.org/abs/2606.23050
- Qwen-AgentWorld: Language World Models for General Agents: https://arxiv.org/abs/2606.24597
- RE-VLM: Event-Augmented Vision-Language Model for Scene Understanding: https://arxiv.org/abs/2605.19329
- 20/20 Vision Language Models: A Prescription for Better VLMs through Data Curation Alone: https://arxiv.org/abs/2605.11405
- PaddleOCR-VL-1.6: Expanding the Frontier of Document Parsing with Under-Optimized Region Refinement and Progressive Post-Training: https://arxiv.org/abs/2606.03264
