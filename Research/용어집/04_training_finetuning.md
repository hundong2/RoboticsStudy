# 04. 학습, 튜닝, 정렬 용어

## 학습 기본

| 용어 | 의미 | 논문 독해 포인트 |
|---|---|---|
| Pretraining | 대규모 데이터로 기본 언어 능력을 학습 | 데이터 규모와 objective를 봅니다. |
| Next-token prediction | 다음 토큰을 맞히는 학습 목표 | decoder-only LLM의 기본 objective입니다. |
| Loss | 예측과 정답의 차이를 나타내는 값 | 낮다고 항상 사람 선호가 높은 것은 아닙니다. |
| Gradient | loss를 줄이기 위한 parameter 변화 방향 | optimizer와 안정성 논의에 나옵니다. |
| Optimizer | gradient로 parameter를 갱신하는 알고리즘 | AdamW가 자주 등장합니다. |
| Learning rate | 한 번에 얼마나 크게 갱신할지 정하는 값 | 너무 크면 불안정, 너무 작으면 느립니다. |
| Batch size | 한 번에 학습하는 샘플 수 | 안정성, memory, throughput과 연결됩니다. |
| Epoch | 전체 데이터를 한 번 학습한 단위 | LLM에서는 token budget으로 표현하기도 합니다. |

## Fine-tuning과 Adapter

| 용어 | 의미 | 기억법 |
|---|---|---|
| Fine-tuning | pretrained model을 특정 task나 도메인에 맞게 추가 학습 | 기본기를 직무 교육으로 바꿉니다. |
| SFT | Supervised Fine-Tuning. 지시와 정답 응답 쌍으로 학습 | instruction following의 기본입니다. |
| PEFT | Parameter-Efficient Fine-Tuning | 전체 가중치 대신 일부만 조정합니다. |
| LoRA | low-rank adapter를 추가해 적은 parameter만 학습 | 큰 모델에 얇은 보정판을 붙입니다. |
| QLoRA | quantized model 위에서 LoRA 학습 | 적은 GPU memory로 fine-tuning합니다. |
| Adapter | 기존 모델 사이에 끼우는 작은 학습 모듈 | task별 모듈 교체가 쉽습니다. |
| Prompt tuning | soft prompt embedding만 학습 | 입력 앞에 학습 가능한 힌트를 붙입니다. |

## 정렬과 선호 학습

| 용어 | 의미 | 논문에서 보이면 |
|---|---|---|
| Alignment | 모델 출력을 인간 의도와 안전 기준에 맞추는 과정 | helpful, harmless, honest 같은 기준을 봅니다. |
| RLHF | 인간 선호 보상 모델로 강화학습 | reward model, PPO가 같이 나옵니다. |
| RLAIF | AI 피드백을 사용한 alignment | 인간 라벨 비용을 줄이려는 흐름입니다. |
| DPO | 선호 쌍으로 직접 policy를 최적화 | RL 없이 preference optimization을 합니다. |
| Reward model | 응답의 선호 점수를 예측하는 모델 | reward hacking 위험을 봐야 합니다. |
| Constitutional AI | 원칙 목록을 기준으로 모델 응답을 개선 | safety 논문에서 자주 등장합니다. |

## 데이터 관련 용어

| 용어 | 의미 |
|---|---|
| Instruction dataset | 지시문과 답변으로 구성된 학습 데이터 |
| Synthetic data | 모델이나 규칙으로 생성한 데이터 |
| Data mixture | 여러 출처와 유형의 데이터를 섞은 구성 |
| Data contamination | 평가 데이터가 학습에 섞여 성능이 부풀려지는 문제 |
| Curriculum learning | 쉬운 데이터에서 어려운 데이터로 학습 순서를 설계 |

## 30초 기억 문장

Pretraining은 언어의 기본 분포를 배우는 단계이고, fine-tuning은 원하는 행동 양식으로 모델을 좁히는 단계다. Alignment는 정답률만이 아니라 사람이 원하는 응답인지까지 맞추는 과정이다. 논문을 읽을 때는 "무슨 데이터로", "어떤 objective로", "얼마나 많은 parameter를", "어떤 평가로" 바꿨는지 확인한다.

