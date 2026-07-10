# 2026-07-05 VLM 리서치 노트: 능동 질문자, 게임 에이전트, 위험 데이터

> 기준일: 2026년 7월 5일  
> 대표 이름: **Self-Evolving Visual Questioner**  
> 핵심 질문: VLM은 왜 더 이상 "답변기"로만 두면 안 되는가?

## 오늘의 결론

VLM의 다음 단계는 사용자가 묻는 질문에만 답하는 수동 모델이 아니라, **스스로 정보 결핍을 발견하고 질문을 만들며, 동적 환경에서 행동하고, 위험한 장면을 미리 구분하는 모델**이다. 2026년 7월 5일 흐름은 세 축으로 정리된다.

- 능동 학습: Self-Evolving Visual Questioner
- 에이전트 평가: OmniGameArena
- 위험 상황 데이터 합성: DriveMRP
- 학습 비용 절감: VLV Auto-Encoder
- 공간 인지 한계 진단: Constructive Apraxia

## 왜 필요한가

정적 VQA는 "이미 주어진 질문에 답하는 능력"만 본다. 하지만 실제 로봇, 게임 에이전트, 자율주행 시스템은 다음을 해야 한다.

- 무엇을 더 확인해야 하는지 스스로 질문한다.
- 행동 후 환경이 어떻게 변할지 고려한다.
- 위험한 long-tail 상황을 데이터로 학습한다.
- 공간 배치 착각을 검증한다.

그래서 질문 생성, 에이전트 벤치마크, 위험 데이터 합성, 공간 오류 진단이 같이 중요해진다.

## Top 5 정리

| 순위 | 논문 | 확인된 메타데이터 | 핵심 |
|---:|---|---|---|
| 1 | Self-Evolving Visual Questioner | arXiv 2606.13929, HF Papers | VLM이 unlabeled image에서 스스로 질문 supervision을 생성 |
| 2 | OmniGameArena | arXiv 2606.09826 | UE5 기반 12개 게임에서 VLM game agent 평가 |
| 3 | Vision-Language-Vision Auto-Encoder | arXiv 2507.07104 | T2I diffusion 지식을 VLM captioner로 증류 |
| 4 | DriveMRP | arXiv 2507.02948 | BEV motion simulation으로 high-risk driving data 생성 |
| 5 | Constructive Apraxia | arXiv 2410.03551 | VLM의 공간 구성 장애를 Ponzo illusion류 과제로 분석 |

## 1. Self-Evolving Visual Questioner

이 논문은 VLM을 질문에 답하는 모델이 아니라 **질문을 만드는 모델**로 본다. unlabeled image를 보고 현재 모델이 스스로 질문 후보를 만들고, 유용한 질문을 필터링해 다시 학습에 사용한다.

### 왜 써야 하나

라벨링된 VQA 데이터는 비싸다. 더 큰 문제는 사람이 만든 질문이 항상 모델의 약점을 찌르지 않는다는 점이다. Self-evolving questioner는 모델이 스스로 난이도를 높이는 curriculum을 만들 수 있다.

### 핵심 구조

```text
image -> proposer가 질문 생성 -> filter가 품질/난이도 선별 -> questioner 학습 -> 반복
```

## 2. OmniGameArena

OmniGameArena는 UE5 기반 12개 게임을 사용해 VLM game agent를 평가한다. Solo, PvP, Coop 환경을 포함하며, 단발 점수뿐 아니라 **reflection을 통한 개선 동역학**도 본다.

### 왜 써야 하나

정적 이미지 benchmark는 에이전트 성능을 제대로 말해주지 않는다. 게임 환경은 관측, 계획, 제어, 실패 후 개선이 모두 들어간 작은 현실 실험장이다.

## 3. Vision-Language-Vision Auto-Encoder

VLV Auto-Encoder는 2026년 신규 논문이 아니라 2025년 arXiv 논문이다. 그러나 이번 주 흐름인 "데이터/컴퓨트 절감" 관점에서 계속 중요하다.

### 왜 써야 하나

VLM을 학습하려면 고품질 image-text pair가 대량으로 필요하다. VLV는 이미 학습된 diffusion decoder를 teacher로 사용해 paired text data 의존도를 낮춘다.

## 4. DriveMRP

DriveMRP는 자율주행의 high-risk motion prediction을 위해 BEV motion simulation으로 위험 데이터를 합성한다.

### 왜 써야 하나

사고 직전 상황은 실제 데이터에서 드물다. 하지만 모델은 바로 그 드문 상황을 잘 알아야 한다. DriveMRP는 long-tail risk를 synthetic data로 보강한다.

## 5. Constructive Apraxia

이 논문은 최신 VLM들이 시각적으로 그럴듯한 결과를 만들더라도, 기하학적 구성에서는 인간의 구성 실행증과 비슷한 실패를 보일 수 있음을 지적한다.

### 왜 써야 하나

공간 지능은 "이미지를 설명한다"와 다르다. 로봇이나 CAD, 도면, AR에서는 수평, 평행, 거리, 배치 같은 기하 관계가 정확해야 한다.

## 실습 연결

실습은 7월 10일 폴더의 `weekly_vlm_practice_project`에 통합했다.

- Self-evolving questioner: 질문 생성/필터 루프
- DriveMRP: BEV 위험 점수 계산
- Constructive Apraxia: 공간 관계 오류 검출

## 참고 링크

- Self-Evolving Visual Questioner: https://arxiv.org/abs/2606.13929
- OmniGameArena: https://arxiv.org/abs/2606.09826
- VLV Auto-Encoder: https://arxiv.org/abs/2507.07104
- DriveMRP: https://arxiv.org/abs/2507.02948
- Constructive Apraxia: https://arxiv.org/abs/2410.03551
