# 08. 논문 독해용 학습 전략

## 3회독 전략

| 회독 | 목표 | 할 일 |
|---|---|---|
| 1회독 | 방향 파악 | Abstract, Figure 1, Conclusion만 읽고 문제와 결과를 적습니다. |
| 2회독 | 구조 파악 | Introduction, Method overview, Experiment table을 연결합니다. |
| 3회독 | 재현 가능성 파악 | 세부 수식, implementation detail, ablation, limitation을 봅니다. |

## 용어 암기 카드 형식

각 용어를 아래 형식으로 1분 안에 씁니다.

```text
용어:
한 줄 정의:
왜 필요한가:
비용/한계:
논문에서 같이 나오는 단어:
반대말 또는 헷갈리는 말:
내 말로 비유:
```

예시:

```text
용어: KV cache
한 줄 정의: 이전 token의 Key/Value를 저장해 decode 때 재사용하는 메모리.
왜 필요한가: 과거 token의 K/V를 매번 다시 계산하지 않기 위해.
비용/한계: 긴 context와 batch에서 GPU memory를 많이 쓴다.
논문에서 같이 나오는 단어: prefill, decode, eviction, compression, PagedAttention.
반대말 또는 헷갈리는 말: model parameter, activation, prefix cache.
내 말로 비유: 읽은 책의 목차와 밑줄을 책상 위에 펼쳐 놓는 것.
```

## 논문 문장 분해법

논문 문장을 만나면 다음 순서로 분해합니다.

1. **무엇을 줄였나**: latency, memory, FLOPs, data, human labels.
2. **무엇을 늘렸나**: accuracy, throughput, context length, robustness.
3. **어떤 비용을 냈나**: quality drop, extra model, preprocessing, hardware dependency.
4. **어디에서만 통하나**: 특정 dataset, 특정 GPU, 특정 prompt 길이.

## Abstract 해석 템플릿

```text
이 논문의 문제:
기존 방법의 병목:
제안 방법:
핵심 용어 3개:
주요 결과:
내가 검증해야 할 의심:
```

## 절대 잊지 않는 반복 루틴

| 시점 | 행동 |
|---|---|
| 읽은 직후 | 용어 5개를 카드 형식으로 씁니다. |
| 1시간 뒤 | 카드의 한 줄 정의만 보고 비용/한계를 떠올립니다. |
| 다음 날 | 논문 abstract를 보지 않고 5문장으로 요약합니다. |
| 3일 뒤 | 다른 논문에서 같은 용어가 어떻게 다르게 쓰였는지 비교합니다. |
| 1주 뒤 | 용어 10개를 연결해 한 장짜리 개념 지도를 만듭니다. |

## LLM/VLM 논문 체크리스트

- 문제는 품질 문제인가, 속도 문제인가, 메모리 문제인가?
- 개선 대상은 model architecture인가, training data인가, inference system인가?
- baseline과 hardware 조건이 공정한가?
- ablation에서 핵심 모듈을 제거했을 때 성능이 떨어지는가?
- long-context, multimodal, agent 설정에서 실제 사용 조건과 얼마나 가까운가?
- 좋은 결과가 특정 benchmark에만 갇혀 있지 않은가?

