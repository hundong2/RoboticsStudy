# Autodata Toy Project

`Autodata: An agentic data scientist to create high quality synthetic data`의 핵심 루프를 작은 Python 코드로 재현한 실습 프로젝트입니다.

## 목적

실제 LLM API 없이 다음 구조를 이해합니다.

```text
challenger -> weak solver -> strong solver -> judge -> recipe update
```

## 실행

```powershell
python Research\2026-07-04_Autodata_Agentic_Data\autodata_toy_project\autodata_toy.py
```

실행 후 `accepted_examples.jsonl`이 생성됩니다.

## 핵심 관찰

- 너무 쉬운 문제는 weak solver도 맞히므로 reject됩니다.
- 너무 어려운 문제는 strong solver도 실패하므로 reject됩니다.
- 적절한 문제는 weak solver는 틀리고 strong solver는 맞히는 gap을 만듭니다.
- judge feedback에 따라 challenger recipe가 더 구체적이고 어려운 방향으로 변합니다.

## 파일

- `autodata_toy.py`: 전체 toy loop 구현
- `sample_sources.json`: source topics
- `accepted_examples.jsonl`: 실행 결과로 생성되는 accepted synthetic data
