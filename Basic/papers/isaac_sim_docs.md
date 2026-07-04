# Isaac Sim 공식 문서 읽기 가이드

- 공식 문서: https://docs.isaacsim.omniverse.nvidia.com/
- 개발자 페이지: https://developer.nvidia.com/isaac/sim
- Isaac Lab: https://isaac-sim.github.io/IsaacLab/

## 왜 읽는가

Isaac Sim은 논문이라기보다 실무 도구다. 로봇 시뮬레이션, 센서 데이터 생성, ROS 2 연동, synthetic data, robot learning 실험에 쓰인다.

## 먼저 볼 문서 범위

1. 설치 요구사항
2. 기본 예제 실행
3. 로봇 import: URDF, USD
4. sensor simulation
5. ROS 2 bridge
6. synthetic data generation
7. Isaac Lab 설치 및 예제

## 핵심 번역식 요약

- Isaac Sim은 물리 기반 가상 환경에서 로봇을 개발, 테스트, 학습하기 위한 시뮬레이션 플랫폼이다.
- OpenUSD 기반 장면 표현을 사용한다.
- ROS 2와 연결해 실제 로봇 소프트웨어 구조와 유사하게 테스트할 수 있다.
- Isaac Lab은 Isaac Sim 위에서 robot learning 워크플로우를 구성하는 프레임워크다.

## 확인 질문

- Gazebo와 Isaac Sim의 차이는 무엇인가?
- 실제 센서와 가상 센서의 차이를 어떻게 줄일 수 있는가?
- synthetic data가 모델 성능에 항상 도움이 되는가?
- sim-to-real에서 domain randomization은 왜 쓰는가?
