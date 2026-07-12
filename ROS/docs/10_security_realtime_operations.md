# 10. 보안, 실시간성, 운영

전문가 수준의 ROS 개발은 기능 구현에서 끝나지 않습니다. 로봇이 현장에서 반복적으로 안전하게 동작하려면 보안, 실시간성, 로그, 테스트, 배포 전략이 필요합니다.

## 보안

ROS 2는 DDS 기반 보안 기능과 SROS2 도구를 사용할 수 있습니다.

체크리스트:

- [ ] 노드별 권한 정책을 정의했다.
- [ ] topic/service/action 접근 범위를 제한했다.
- [ ] 로봇과 운영 PC 사이 네트워크를 분리했다.
- [ ] 원격 접속 계정과 키를 관리한다.
- [ ] 로그에 민감 정보가 남지 않게 했다.

## 실시간성

ROS 2는 실시간 운영체제가 아닙니다. 하지만 설계를 잘하면 제어 루프의 지연과 jitter를 줄일 수 있습니다.

확인할 것:

- executor 종류와 callback group 설계
- DDS/RMW 선택
- message allocation과 copy 비용
- controller loop 주기
- CPU isolation, real-time kernel, priority

## 운영과 관측성

운영 가능한 로봇은 실패를 설명할 수 있어야 합니다.

- rosbag2로 핵심 topic을 기록합니다.
- `/diagnostics`를 통해 센서와 노드 상태를 모읍니다.
- 로그 레벨을 상황에 맞게 조절합니다.
- CI에서 unit test, launch test, simulation smoke test를 실행합니다.
- 현장 문제는 "재현 가능한 bag + 설정 파일 + commit"으로 남깁니다.

## 통과 기준

- 기능 테스트와 회귀 테스트의 차이를 설명한다.
- latency 평균과 최악 지연의 차이를 설명한다.
- 보안 설정이 개발 편의성과 충돌할 때 어떤 기준으로 조정할지 말할 수 있다.
