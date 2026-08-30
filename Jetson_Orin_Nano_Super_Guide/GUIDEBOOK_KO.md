# Jetson Orin Nano Super Vision VLA 가이드북

기준일: 2026-08-29  
기준 스택: JetPack 7.2.1 / Jetson Linux 39.2.1 / Ubuntu 24.04 / ROS 2 Jazzy

## 이 책의 목적

Jetson Orin Nano Super Developer Kit의 하드웨어 연결, 핀 사용, JetPack/BSP 설치, 커스텀 캐리어보드 브링업, 카메라·Vision 가속, ROS 2/Isaac ROS, 임베디드 Vision VLA 제품화까지 하나의 흐름으로 안내한다.

이 통합 파일은 문서 생성용 목차다. 상세 본문은 아래 장 파일을 순서대로 결합한다.

1. [하드웨어와 전원](docs/01_하드웨어와_전원.md)
2. [핀과 커넥터](docs/02_핀과_커넥터.md)
3. [JetPack과 BSP 설치](docs/03_JetPack_BSP_설치.md)
4. [커스텀 BSP와 브링업](docs/04_커스텀_BSP_브링업.md)
5. [카메라와 Vision](docs/05_카메라와_Vision.md)
6. [AI 가속과 최적화](docs/06_AI_가속과_최적화.md)
7. [ROS 2와 Isaac ROS](docs/07_ROS2_Isaac_ROS.md)
8. [Vision VLA 로드맵](docs/08_Vision_VLA_로드맵.md)
9. [운영·안전·문제 해결](docs/09_운영_안전_문제해결.md)
10. [공식 문서 목록](references/공식문서_목록.md)
11. [커버리지 매트릭스](references/커버리지_매트릭스.md)

## 핵심 결론

- J14 전원 버튼은 11(GND)-12(SLEEP/WAKE*) 순간식 버튼, 자동 부팅 비활성화는 5-6 점퍼다.
- J12 40핀 신호는 3.3V이며, GPIO에 5V를 인가하거나 모터·릴레이를 직접 구동하면 안 된다.
- 최신 첫 설치는 JetPack 7.2.1 Jetson ISO이며 ISO를 microSD에 직접 기록하지 않는다.
- 오래된 출고 QSPI는 JetPack 6.x 업데이트 경로를 먼저 거친다.
- Orin NX/Nano의 제품용 플래시는 initrd flash를 기준으로 한다.
- VLA는 소형 VLM, 실시간 perception, 제한된 action schema, 독립 safety supervisor로 계층화한다.
- 8GB 메모리에서는 NVMe, 컨테이너 버전 고정, zero-copy 파이프라인, 양자화가 핵심이다.

