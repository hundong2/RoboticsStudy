# 08. 검증 리포트

이 문서는 AI 로봇 C++/ROS2 실무 파이프라인 문서를 3회 검증한 결과를 기록합니다.

## 검증 명령

```bash
python ROS/ai_robotics_cpp_pipeline/scripts/validate_ai_robotics_cpp_pipeline_docs.py
```

## 1차 검증

결과: 통과

```text
[PASS] required files: all required files exist
[PASS] markdown links: all local markdown links resolve
[PASS] glossary anchors: all glossary anchors resolve
[PASS] required glossary terms: all required terms exist
[PASS] keyword coverage: all requested topics covered
[PASS] python syntax: all Python examples parse
[PASS] cpp comment density: C++ examples are heavily commented
```

점검 내용:

- 필수 문서와 코드 예제가 모두 존재합니다.
- 문서 내부 상대 링크가 깨지지 않았습니다.
- 사전 링크의 anchor가 실제 용어 사전 heading과 연결됩니다.

## 2차 검증

결과: 통과

```text
[PASS] required files: all required files exist
[PASS] markdown links: all local markdown links resolve
[PASS] glossary anchors: all glossary anchors resolve
[PASS] required glossary terms: all required terms exist
[PASS] keyword coverage: all requested topics covered
[PASS] python syntax: all Python examples parse
[PASS] cpp comment density: C++ examples are heavily commented
```

점검 내용:

- C++/Python/ROS2, LLM/NLP, LangChain, LlamaIndex, VLA, Isaac Sim, PyBullet, MuJoCo, ONNX Runtime, TensorRT, dry-run, closed-loop, sim-to-real 키워드가 모두 문서에 포함됩니다.
- 초보자가 모르는 용어를 [용어 사전](../glossary/README.md)에서 찾을 수 있도록 핵심 용어를 링크했습니다.

## 3차 검증

결과: 통과

```text
[PASS] required files: all required files exist
[PASS] markdown links: all local markdown links resolve
[PASS] glossary anchors: all glossary anchors resolve
[PASS] required glossary terms: all required terms exist
[PASS] keyword coverage: all requested topics covered
[PASS] python syntax: all Python examples parse
[PASS] cpp comment density: C++ examples are heavily commented
```

점검 내용:

- Python Agent 예제는 `ast.parse` 기준 문법 오류가 없습니다.
- C++ 예제는 초보자용 라인 단위 설명을 위해 주석 밀도 검사를 통과했습니다.
- 무로봇 시뮬레이션과 실제 로봇 테스트 단계가 문서에서 분리되어 설명됩니다.

## 최종 개선 판단

완료.

3회 검증 기준에서 문서 구조, 내부 링크, 사전 anchor, 필수 용어, 요청 키워드, Python 문법, C++ 주석 밀도가 모두 통과했습니다.

개선된 점:

- [README.md](../README.md)에 전체 파이프라인을 한눈에 볼 수 있는 목차를 추가했습니다.
- [glossary/README.md](../glossary/README.md)에 ROS/AI/시뮬레이터/배포 용어를 사전형으로 정리했습니다.
- [ml_policy_node.cpp](../examples/cpp_ml_policy_node/src/ml_policy_node.cpp)와 [safety_filter.cpp](../examples/cpp_ml_policy_node/src/safety_filter.cpp)에 라인 단위 주석을 추가했습니다.
- [agent_command_bridge.py](../examples/python_agent_bridge/agent_command_bridge.py)에 LLM Agent 명령을 안전한 JSON 계획으로 바꾸는 예제를 추가했습니다.
- [validate_ai_robotics_cpp_pipeline_docs.py](../scripts/validate_ai_robotics_cpp_pipeline_docs.py)로 향후 문서 품질을 계속 점검할 수 있게 했습니다.
