# 04. LLM Agent 기반 로봇 응용

[LLM](../glossary/README.md#llm), [NLP](../glossary/README.md#nlp), [LangChain](../glossary/README.md#langchain), [LlamaIndex](../glossary/README.md#llamaindex)는 로봇에서 자연어 명령, 작업 계획, 문서 검색, 스크립트 자동생성에 쓰일 수 있습니다. 그러나 LLM 출력은 확률적이므로 직접 모터 명령으로 연결하면 안 됩니다.

## 안전한 구조

```text
사용자 자연어 명령
  -> LLM Agent
  -> JSON 작업 계획
  -> schema validation
  -> rule-based safety check
  -> ROS 2 action/service call
  -> robot execution
```

## Agent가 해도 되는 일과 안 되는 일

| 허용 | 금지 |
|---|---|
| 로봇 상태 조회 | `/cmd_vel` 직접 publish |
| 매뉴얼/RAG 검색 | safety filter 우회 |
| 작업 계획 JSON 생성 | 검증 안 된 Python/C++ 코드 즉시 실행 |
| ROS launch/script 초안 작성 | 실제 로봇에서 임의 속도 명령 |
| 실패 로그 요약 | emergency stop 무시 |

## LangChain/LlamaIndex 역할 분리

| 도구 | 주 용도 |
|---|---|
| [LangChain](../glossary/README.md#langchain) | tool-calling agent, guardrail, agent loop |
| [LlamaIndex](../glossary/README.md#llamaindex) | 로봇 매뉴얼, 점검 절차, 실험 로그 검색/RAG |
| ROS 2 bridge code | agent 계획을 service/action 요청으로 변환 |

## Python Agent 예제

- [agent_command_bridge.py](../examples/python_agent_bridge/agent_command_bridge.py)

이 예제는 자연어를 바로 로봇 명령으로 바꾸지 않고, 안전 검증 가능한 JSON 계획으로 바꾸는 최소 구조를 보여줍니다.

## 생성형 AI 기반 스크립트 자동생성 원칙

1. LLM은 초안을 생성합니다.
2. schema validator가 구조를 검사합니다.
3. static rule이 위험 명령을 차단합니다.
4. 시뮬레이터에서 dry-run합니다.
5. 사람이 승인해야 실제 로봇으로 갑니다.

## 통과 기준

- [LLM Agent](../glossary/README.md#llm-agent)가 직접 actuator를 제어하면 위험한 이유를 설명한다.
- [RAG](../glossary/README.md#rag)가 로봇 매뉴얼과 작업 절차 검색에 어떻게 쓰이는지 설명한다.
- 생성형 AI가 만든 스크립트는 시뮬레이터와 사람이 검토한 뒤 실제 로봇에 적용해야 함을 설명한다.
