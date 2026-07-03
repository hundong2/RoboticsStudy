# LLM/VLM 용어집

이 폴더는 LLM, VLM, Transformer, KV cache, 추론 최적화 논문을 읽기 위한 용어집입니다. 목표는 용어를 단순 암기가 아니라 "논문 문장 안에서 바로 해석되는 패턴"으로 익히는 것입니다.

## 파일 목록

- [01_llm_core.md](./01_llm_core.md): LLM 기본 구조와 생성 방식
- [02_attention_transformer.md](./02_attention_transformer.md): Attention과 Transformer 핵심
- [03_kv_cache_inference.md](./03_kv_cache_inference.md): KV cache와 추론 시스템
- [04_training_finetuning.md](./04_training_finetuning.md): 학습, 튜닝, 정렬
- [05_efficiency_systems.md](./05_efficiency_systems.md): 효율화, 병렬화, 서빙
- [06_evaluation_benchmarks.md](./06_evaluation_benchmarks.md): 평가 지표와 벤치마크
- [07_multimodal_vlm.md](./07_multimodal_vlm.md): VLM과 멀티모달 모델
- [08_paper_reading_strategy.md](./08_paper_reading_strategy.md): 논문 독해용 학습 전략

## 절대 잊지 않는 학습 전략

용어 하나를 볼 때마다 다음 4칸으로 정리합니다.

| 칸 | 질문 | 예시 |
|---|---|---|
| 정의 | 이게 무엇인가? | KV cache는 이전 토큰의 Key/Value를 저장한 메모리다. |
| 왜 필요한가 | 어떤 병목을 줄이나? | 매 토큰마다 과거 토큰의 K/V를 다시 계산하지 않게 한다. |
| 비용 | 무엇을 더 쓰나? | 긴 context에서 GPU memory를 많이 쓴다. |
| 논문 신호 | 어떤 문장에 나오나? | "reduces decoding latency", "cache eviction", "long-context inference" |

암기 순서는 다음처럼 고정합니다.

1. **그림으로 먼저 기억**: Query는 지금 묻는 토큰, Key/Value는 과거 토큰의 색인과 내용이라고 그립니다.
2. **반대말로 고정**: prefill vs decode, training vs inference, throughput vs latency처럼 쌍으로 외웁니다.
3. **논문 문장에 붙이기**: abstract에서 용어가 나오면 "문제", "방법", "결과" 중 어디에 쓰였는지 표시합니다.
4. **30초 설명**: 용어를 "초등학생 설명", "개발자 설명", "논문식 설명" 3단계로 말합니다.
5. **실패 사례까지 기억**: KV cache는 빠르게 하지만 memory pressure를 만든다는 식으로 장점과 비용을 같이 외웁니다.

## 논문 읽기 최소 루틴

1. Abstract에서 성능 향상 단어를 찾습니다: speedup, accuracy, memory, latency, throughput.
2. Introduction에서 기존 병목을 찾습니다: compute-bound, memory-bound, long-context, hallucination.
3. Method에서 새로 정의한 모듈 이름을 찾습니다.
4. Experiment에서 baseline, metric, dataset, ablation을 확인합니다.
5. Limitations에서 실제 적용 위험을 확인합니다.

