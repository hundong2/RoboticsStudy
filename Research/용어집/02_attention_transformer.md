# 02. Attention과 Transformer 용어

## Attention 핵심

| 용어 | 의미 | 기억법 |
|---|---|---|
| Attention | 각 토큰이 다른 토큰을 얼마나 참고할지 계산하는 구조 | "어디를 볼지 정하는 가중치"입니다. |
| Query | 현재 토큰이 찾고 싶은 정보 | 질문 |
| Key | 각 토큰이 가진 색인 | 목차 |
| Value | 실제로 가져올 정보 | 본문 |
| QK score | Query와 Key의 유사도 점수 | 질문과 목차의 매칭 점수 |
| Softmax attention | score를 확률처럼 정규화해 Value를 섞음 | 많이 맞는 곳을 더 크게 반영합니다. |
| Attention head | 서로 다른 관점의 attention 계산 단위 | 한 head는 문법, 다른 head는 위치를 볼 수 있습니다. |
| Multi-head attention | 여러 head를 병렬로 사용 | 여러 관점으로 같은 문장을 읽습니다. |
| Self-attention | 같은 sequence 안에서 attention | LLM의 기본입니다. |
| Cross-attention | 다른 sequence를 참조하는 attention | encoder-decoder, VLM에서 자주 나옵니다. |

## Transformer 구조

| 용어 | 의미 | 논문 독해 포인트 |
|---|---|---|
| Transformer block | attention과 MLP가 묶인 반복 단위 | layer 수가 깊이와 비용을 결정합니다. |
| LayerNorm | activation 분포를 안정화하는 정규화 | pre-norm/post-norm 구조 차이를 봅니다. |
| Residual connection | 입력을 출력에 더하는 skip 연결 | 깊은 모델 학습 안정성의 핵심입니다. |
| MLP/FFN | attention 뒤의 비선형 변환층 | LLM parameter의 큰 비중을 차지합니다. |
| Positional encoding | token 순서 정보를 넣는 방식 | 긴 context 확장 논문에서 중요합니다. |
| RoPE | 회전 위치 임베딩 | long-context scaling에서 자주 수정됩니다. |
| ALiBi | attention score에 거리 bias를 주는 방식 | 긴 길이 일반화 논문에서 등장합니다. |
| Causal mask | 미래 토큰을 보지 못하게 막는 mask | autoregressive LLM의 필수 조건입니다. |

## 자주 헷갈리는 용어

| 용어 쌍 | 구분 |
|---|---|
| Attention weight vs Model weight | attention weight는 입력마다 달라지는 참고 비율, model weight는 학습된 parameter입니다. |
| Key/Value vs Query | Query는 새 토큰마다 계산되는 질문, Key/Value는 과거 토큰의 저장 가능한 정보입니다. |
| Encoder-only vs Decoder-only | BERT 계열은 encoder-only, GPT 계열 LLM은 decoder-only가 기본입니다. |
| Full attention vs Sparse attention | full은 모든 토큰 쌍을 보고, sparse는 일부 연결만 봅니다. |

## 논문 문장 해석 패턴

- "quadratic complexity in sequence length": attention 비용이 토큰 길이의 제곱으로 커진다는 뜻입니다.
- "linear attention": attention 계산을 길이에 선형으로 줄이려는 방법입니다.
- "attention sink": 일부 초기 토큰에 attention이 과도하게 몰리는 현상입니다.
- "head pruning": 중요하지 않은 attention head를 제거해 계산량을 줄입니다.

