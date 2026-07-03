# 06. 평가 지표와 벤치마크 용어

## 일반 평가 용어

| 용어 | 의미 | 주의점 |
|---|---|---|
| Benchmark | 모델 성능을 비교하기 위한 표준 데이터와 절차 | 데이터 오염 가능성을 봅니다. |
| Baseline | 비교 대상 기존 방법 | baseline이 약하면 개선이 과장됩니다. |
| SOTA | State of the Art. 현재 최고 수준 | 특정 benchmark에서만 최고일 수 있습니다. |
| Ablation study | 구성 요소를 제거하며 효과를 확인 | method의 핵심 기여를 검증합니다. |
| Human evaluation | 사람이 직접 품질을 평가 | 비용이 크고 평가 기준 편향이 있을 수 있습니다. |
| Win rate | 두 모델 응답 중 선호된 비율 | judge 모델이나 사람 기준을 확인합니다. |
| Statistical significance | 결과 차이가 우연인지 검증 | 작은 차이는 신뢰 구간을 봐야 합니다. |

## 언어 모델 평가

| 용어 | 의미 |
|---|---|
| Perplexity | 모델이 다음 토큰을 얼마나 잘 예측하는지 나타내는 지표 |
| Accuracy | 정답률 |
| Exact match | 예측 문자열이 정답과 완전히 같은 비율 |
| F1 score | precision과 recall의 조화 평균 |
| BLEU | 번역 품질 평가에 쓰이는 n-gram 기반 지표 |
| ROUGE | 요약 평가에 쓰이는 overlap 기반 지표 |
| Pass@k | k개 생성 중 하나라도 테스트를 통과할 확률 |

## LLM 벤치마크에서 읽을 것

| 평가 유형 | 봐야 할 질문 |
|---|---|
| 지식 QA | 학습 데이터 암기인지 추론인지 구분되는가? |
| 수학 | chain-of-thought, tool use 여부가 동일한가? |
| 코딩 | 실행 기반 평가인지 문자열 비교인지 확인합니다. |
| 장문 문맥 | 긴 context 전체를 실제로 쓰는지 needle test만 보는지 확인합니다. |
| 안전성 | refusal만 늘어난 것인지 실제 위험 응답이 줄었는지 봅니다. |

## 시스템 평가

| 용어 | 의미 |
|---|---|
| Latency | 요청 하나의 응답 지연 |
| TTFT | 첫 토큰까지 걸리는 시간 |
| TPOT | 출력 토큰 하나당 시간 |
| Throughput | 초당 처리 token 또는 request 수 |
| Tokens/sec | 초당 생성 또는 처리 token 수 |
| Memory footprint | 실행에 필요한 메모리 사용량 |
| Cost per token | token 하나 처리에 드는 비용 |

## 30초 기억 문장

평가 표를 볼 때는 최고 점수만 보지 말고 baseline, metric, dataset, prompt format, decoding setting, hardware를 같이 봐야 한다. LLM 논문은 작은 설정 차이로 성능표가 크게 달라질 수 있다.

