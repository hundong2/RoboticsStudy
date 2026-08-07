# Python Agent Bridge 예제

이 예제는 [LLM Agent](../../glossary/README.md#llm-agent)가 만든 자연어 기반 계획을 바로 로봇 명령으로 보내지 않고, 검증 가능한 JSON 계획으로 바꾸는 구조를 보여줍니다.

실제 LangChain/LlamaIndex 연결 전, 먼저 schema validation과 safety rule을 테스트하는 목적으로 사용합니다.

```bash
python agent_command_bridge.py "입구로 이동하고 장애물을 피해서 대기해"
```
