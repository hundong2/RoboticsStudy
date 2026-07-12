# 05. 캡스톤: 자율 점검 로봇

이 프로젝트는 전문가 수준 도달 여부를 검증하기 위한 통합 과제입니다.

## 요구사항

로봇은 지정된 구역으로 이동하고, 카메라로 점검 대상을 촬영하고, 탐지 결과와 주행 로그를 남겨야 합니다.

## ROS 그래프 초안

```text
/camera/image_raw -> perception_node -> /inspection/detections
/scan             -> nav2_costmap
/odom             -> robot_localization 또는 Nav2
/tf               -> RViz, Nav2, perception transform
/goal_pose        -> Nav2 action server
/cmd_vel          -> base controller
```

## 필수 산출물

- architecture.md: 시스템 구조와 데이터 흐름
- interfaces.md: topic/service/action/parameter 목록
- tf_tree.md: frame 관계와 책임 노드
- launch/: bringup launch 파일
- bags/: 실패 재현용 rosbag2 설명
- tests/: 단위 테스트와 시뮬레이션 smoke test 설명
- report.md: 실험 결과, 실패 분석, 개선 계획

## 평가 루브릭

| 영역 | 초급 | 중급 | 고급 | 전문가 |
|---|---|---|---|---|
| ROS 그래프 | 노드 실행 | topic 관찰 | QoS/TF 디버깅 | 장애 재현과 회귀 테스트 |
| 주행 | 수동 이동 | SLAM | Nav2 목표 이동 | 실패 복구와 costmap 튜닝 |
| perception | 이미지 보기 | 탐지 노드 실행 | 좌표 변환 | latency/성능 계측 |
| 운영 | 로그 확인 | rosbag 기록 | CI smoke test | 배포판/RMW/보안 근거 |

## 합격 조건

- 최소 1개 미션 성공 영상 또는 실행 로그
- 최소 1개 실패를 rosbag2로 재현
- TF, topic, parameter, launch를 문서화
- 최신 ROS 2 기능 중 1개 이상을 선택하거나 제외한 근거 작성
