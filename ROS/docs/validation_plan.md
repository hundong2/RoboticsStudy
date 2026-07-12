# 검증 계획

목표: 기초 지식이 없는 학습자가 이 자료를 따라갔을 때 기초에서 전문가 수준까지 도달 가능한지 문서 구조, 실습 연결성, 코드 이해 가능성, 평가 기준을 검증합니다.

## 검증 관점

| 관점 | 검증 질문 | 방법 |
|---|---|---|
| 학습 경로 | 입문자가 어디서 시작할지 명확한가? | README 커리큘럼과 단계별 문서 확인 |
| 개념 누락 | Node, Topic, Service, Action, Parameter, TF가 모두 다뤄지는가? | 문서 키워드 검사 |
| 실습 연결 | 각 단계에 실행하거나 설계할 프로젝트가 있는가? | README 링크와 project README 확인 |
| 코드 주석 | 코드가 초보자 문법 설명을 충분히 포함하는가? | Python 파일 comment/docstring 비율 확인 |
| 최신성 | 2026 최신 ROS 2 흐름이 반영됐는가? | Lyrical, Jazzy, Kilted, Zenoh, zero-copy, Physical AI 언급 확인 |
| 무로봇 실습 | 실제 로봇이 없어도 학습 가능한가? | Gazebo, Webots, Isaac Sim, MuJoCo, PyBullet, Drake, CoppeliaSim, CARLA 실습 경로 확인 |
| 평가 가능성 | 전문가 수준 도달 여부를 판단할 기준이 있는가? | capstone, checklist, validation docs 확인 |
| 저장소 연결 | 최상단 README Todo에 ROS 링크가 있는가? | root README 링크 검사 |

## 3회 검증 절차

1. 1차 구조 검증: 필수 파일과 내부 링크가 존재하는지 확인합니다.
2. 2차 학습자 관점 검증: 초보자가 순서대로 읽을 때 선행 개념 없이 튀는 부분이 있는지 확인합니다.
3. 3차 실행/유지보수 검증: Python 예제 문법, 패키지 메타데이터, 검증 스크립트 재실행 가능성을 확인합니다.

## 자동 검증

```bash
python ROS/scripts/validate_ros_learning_materials.py
```

검증 스크립트는 ROS 설치 없이 실행되도록 만들었습니다. 학습 자료 자체의 구조와 코드 문법을 검사하기 위한 도구입니다.
