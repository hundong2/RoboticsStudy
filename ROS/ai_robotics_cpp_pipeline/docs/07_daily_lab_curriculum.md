# 07. 데일리 실습 커리큘럼

이 커리큘럼은 초보자가 8주 동안 매일 작게 실습하며 C++/Python/ROS2/AI Agent/시뮬레이터/임베디드 최적화를 연결하도록 설계했습니다.

## 8주 로드맵

| 주차 | 주제 | 산출물 |
|---:|---|---|
| 1 | ROS 2 생태계와 CLI | topic graph, 용어 카드 |
| 2 | C++ ROS 2 node | publisher/subscriber 예제 |
| 3 | PyTorch -> ONNX -> C++ 추론 | ONNX model, inference log |
| 4 | Gazebo/Isaac/PyBullet/MuJoCo 무로봇 테스트 | closed-loop plan |
| 5 | safety filter와 rosbag2 회귀 테스트 | failure replay |
| 6 | LLM Agent와 RAG 기반 로봇 작업 계획 | JSON plan validator |
| 7 | 실제 로봇 테스트 절차 | dry-run checklist |
| 8 | ONNX Runtime/TensorRT 최적화 | latency report |

## 매일 루틴

| 시간 | 활동 | 산출물 |
|---:|---|---|
| 20분 | 용어 사전 5개 읽기 | 링크된 용어 체크 |
| 40분 | 공식 문서 1개 읽기 | 5문장 요약 |
| 60분 | 코드 한 파일 작성/수정 | 실행 로그 |
| 40분 | 시뮬레이션 또는 bag replay | topic 캡처 |
| 30분 | 실패 분석 | 다음 실험 1개 |

## 매주 평가 질문

1. 이번 주 코드는 실제 로봇 없이 어떻게 검증했는가?
2. 같은 입력 bag을 다시 넣으면 같은 출력이 나오는가?
3. 모델 출력과 최종 제어 명령 사이에 안전 계층이 있는가?
4. LLM이 생성한 계획을 사람이 읽고 검증할 수 있는가?
5. 실제 로봇으로 옮길 때 가장 위험한 gap은 무엇인가?

## 최종 프로젝트

```text
자연어 명령
  -> LLM Agent JSON 계획
  -> C++ ML policy node
  -> safety filter
  -> simulator closed-loop
  -> rosbag2 replay
  -> real robot dry-run plan
  -> embedded latency report
```

## 통과 기준

- 초보자는 용어를 사전에서 찾으며 문서를 끝까지 읽을 수 있다.
- 중급자는 C++ ROS 2 추론 노드와 safety filter를 설명할 수 있다.
- 고급자는 시뮬레이션 평가와 실제 로봇 테스트 계획을 분리해 설계할 수 있다.
