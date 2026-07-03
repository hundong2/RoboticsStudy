# 01. LLM 기본 용어

## 핵심 모델 용어

| 용어 | 의미 | 논문에서 보이면 |
|---|---|---|
| LLM | 대규모 언어 모델. 대량 텍스트로 학습한 next-token predictor | 모델 크기, 데이터, 추론 비용을 같이 봅니다. |
| Token | 모델이 처리하는 최소 단위. 단어, 부분 단어, 문자 조각일 수 있음 | context length, token budget, throughput 계산의 단위입니다. |
| Vocabulary | 모델이 출력할 수 있는 토큰 집합 | tokenizer가 다르면 같은 문장도 토큰 수가 달라집니다. |
| Embedding | 토큰을 dense vector로 바꾼 표현 | 의미 공간, similarity, retrieval과 연결됩니다. |
| Hidden state | 각 layer를 지나며 갱신되는 토큰 표현 | representation, activation 분석에서 자주 나옵니다. |
| Logit | softmax 전의 원점수 | decoding, calibration, preference optimization에서 중요합니다. |
| Probability distribution | 다음 토큰 후보에 대한 확률 분포 | sampling 전략과 uncertainty를 읽는 기준입니다. |
| Autoregressive | 이전 토큰을 조건으로 다음 토큰을 하나씩 생성하는 방식 | decoding latency가 토큰 수에 비례하기 쉽습니다. |
| Context window | 모델이 한 번에 볼 수 있는 최대 토큰 길이 | long-context 논문은 품질과 메모리 비용을 함께 봐야 합니다. |
| Prompt | 모델 입력 지시문 | prompt engineering, instruction following 성능과 연결됩니다. |

## 생성 관련 용어

| 용어 | 의미 | 기억법 |
|---|---|---|
| Decoding | logit에서 실제 출력 토큰을 고르는 과정 | "점수표에서 답 고르기"입니다. |
| Greedy decoding | 매번 가장 확률 높은 토큰만 선택 | 빠르고 결정적이지만 다양성이 낮습니다. |
| Beam search | 여러 후보 문장을 동시에 유지하며 탐색 | 번역식 task에 강하지만 chat LLM에는 항상 최선은 아닙니다. |
| Sampling | 확률에 따라 토큰을 뽑는 방식 | 창의성은 올리고 재현성은 낮춥니다. |
| Temperature | 확률 분포를 날카롭게/부드럽게 하는 값 | 낮으면 보수적, 높으면 다양합니다. |
| Top-k | 상위 k개 후보에서만 sampling | 이상한 꼬리 후보를 자릅니다. |
| Top-p | 누적 확률 p까지의 후보에서 sampling | 후보 개수가 상황에 따라 변합니다. |
| Repetition penalty | 같은 표현 반복을 줄이는 보정 | 긴 생성에서 루프를 줄입니다. |
| Stop sequence | 생성을 멈추는 문자열 또는 토큰 | API, agent, tool call에서 중요합니다. |

## 논문에서 자주 나오는 구분

| 쌍 | 차이 |
|---|---|
| Model vs System | model은 신경망 자체, system은 serving, cache, scheduler, hardware까지 포함합니다. |
| Parameter vs Activation | parameter는 고정 가중치, activation은 입력마다 생기는 중간값입니다. |
| Training vs Inference | training은 가중치 업데이트, inference는 학습된 모델 사용입니다. |
| Prompting vs Fine-tuning | prompting은 입력만 바꾸고, fine-tuning은 가중치를 바꿉니다. |
| Latency vs Throughput | latency는 한 요청의 지연, throughput은 시간당 처리량입니다. |

## 30초 기억 문장

LLM은 지금까지의 token을 보고 다음 token의 logit을 만들고, decoding 전략이 그 logit에서 실제 token을 고른다. 논문에서 성능이 좋아졌다고 하면 "모델 표현이 좋아진 것인지", "decoding이 좋아진 것인지", "serving system이 빨라진 것인지"를 먼저 분리한다.

