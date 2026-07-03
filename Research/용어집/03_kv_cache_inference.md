# 03. KV Cache와 추론 시스템 용어

## KV cache 기본

| 용어 | 의미 | 절대 기억법 |
|---|---|---|
| KV cache | 이전 토큰의 Key와 Value를 저장한 메모리 | "과거 토큰의 목차와 본문을 보관"합니다. |
| Prefill | prompt 전체를 한 번에 처리해 첫 KV cache를 만드는 단계 | 긴 prompt를 읽는 단계입니다. |
| Decode | 새 토큰을 하나씩 생성하며 cache를 갱신하는 단계 | 한 글자씩 쓰는 단계입니다. |
| Cache hit | 필요한 KV가 이미 cache에 있는 상태 | 다시 계산하지 않아 빠릅니다. |
| Cache miss | 필요한 KV가 없어 계산하거나 불러와야 하는 상태 | 지연이 늘어납니다. |
| Cache eviction | cache 공간 확보를 위해 일부 KV를 버림 | 긴 대화에서 무엇을 버릴지가 핵심입니다. |
| KV compression | KV cache를 압축해 메모리 사용량을 줄임 | 품질 손실과 메모리 절감의 trade-off입니다. |
| PagedAttention | KV cache를 고정 크기 page로 관리하는 방식 | OS 가상 메모리처럼 cache 조각을 관리합니다. |
| Continuous batching | 들어오는 요청을 동적으로 batch에 섞어 처리 | serving throughput을 높입니다. |

## 왜 중요한가

Autoregressive LLM은 새 token을 만들 때 과거 token 전체를 참고합니다. KV cache가 없으면 매번 과거 전체의 Key/Value를 다시 계산해야 합니다. KV cache는 compute를 줄이지만, 긴 context와 많은 동시 요청에서 GPU memory를 크게 압박합니다.

## 논문에서 보는 비용 공식

| 항목 | 증가 조건 | 영향 |
|---|---|---|
| KV cache memory | layer 수, head 수, hidden size, sequence length, batch size 증가 | GPU memory 부족 |
| Prefill latency | prompt 길이 증가 | 첫 토큰까지 시간이 늘어남 |
| Decode latency | 생성 token 수 증가 | 사용자 체감 응답 시간이 늘어남 |
| Throughput | batching, scheduler, kernel 최적화 | 서버 처리량 증가 |

## 관련 용어

| 용어 | 의미 | 논문 신호 |
|---|---|---|
| TTFT | Time To First Token. 요청 후 첫 토큰까지 시간 | prefill 최적화 논문에서 중요합니다. |
| TPOT | Time Per Output Token. 출력 토큰 하나당 시간 | decode 최적화 지표입니다. |
| Memory bandwidth | 메모리에서 데이터를 읽고 쓰는 속도 | decode가 memory-bound라는 표현과 연결됩니다. |
| Compute-bound | 연산량이 병목인 상태 | GPU 연산 성능 개선이 효과적입니다. |
| Memory-bound | 메모리 이동이 병목인 상태 | cache layout, quantization이 중요합니다. |
| Speculative decoding | 작은 draft model이 후보를 만들고 큰 model이 검증 | decode latency를 줄입니다. |
| Prefix caching | 같은 prompt prefix의 KV cache를 재사용 | RAG, system prompt, multi-turn chat에서 효과적입니다. |
| Sliding window attention | 최근 일부 token만 보도록 제한 | memory를 줄이지만 먼 문맥 손실이 생깁니다. |
| Chunking | 긴 sequence를 작은 덩어리로 나눠 처리 | long-context와 streaming에서 자주 씁니다. |

## 헷갈리는 쌍

| 쌍 | 구분 |
|---|---|
| KV cache vs Model parameter | KV cache는 요청마다 생기는 임시 메모리, parameter는 모델 파일에 저장된 가중치입니다. |
| Prefill vs Decode | prefill은 prompt를 읽는 병렬 단계, decode는 token을 순차 생성하는 단계입니다. |
| Latency vs Throughput | KV cache 최적화가 한 요청 latency와 서버 throughput에 다르게 작용할 수 있습니다. |
| Cache compression vs Quantization | compression은 저장 표현을 줄이는 넓은 개념, quantization은 낮은 bit precision을 쓰는 대표 방법입니다. |

## 30초 기억 문장

KV cache는 LLM 추론에서 과거 token의 Key/Value를 저장해 반복 계산을 없애는 장치다. 하지만 긴 context와 많은 사용자 요청에서는 cache 자체가 GPU memory 병목이 된다. 그래서 KV cache 논문은 대부분 "무엇을 저장하고, 무엇을 버리고, 어떻게 배치해, 품질 손실 없이 latency와 memory를 줄일까"를 다룬다.

