# Jetson Orin Nano Super 개발 가이드 (한국어)

이 폴더는 NVIDIA Jetson Orin Nano Super Developer Kit를 임베디드 Vision VLA 플랫폼으로 구성하기 위한 한국어 실무 가이드입니다. 기준일은 2026-08-29이며, 주 기준 소프트웨어는 JetPack 7.2.1 / Jetson Linux 39.2.1입니다.

## 먼저 읽을 순서

1. `docs/01_하드웨어와_전원.md`
2. `docs/02_핀과_커넥터.md`
3. `docs/03_JetPack_BSP_설치.md`
4. `docs/04_커스텀_BSP_브링업.md`
5. `docs/05_카메라와_Vision.md`
6. `docs/06_AI_가속과_최적화.md`
7. `docs/07_ROS2_Isaac_ROS.md`
8. `docs/08_Vision_VLA_로드맵.md`
9. `docs/09_운영_안전_문제해결.md`
10. `references/공식문서_목록.md`와 `references/커버리지_매트릭스.md`

통합본은 `output/Jetson_Orin_Nano_Super_Vision_VLA_가이드북.docx`와 PDF로 제공됩니다.

캐리어보드 사양서 v1.3의 장·표별 한국어 번역·해설은 다음 파일로 제공합니다.

- `translations/Jetson_Orin_Nano_DevKit_Carrier_Board_Spec_v1.3_KO.md`
- `output/Jetson_Orin_Nano_Carrier_Board_Spec_v1.3_KO.docx`
- `output/Jetson_Orin_Nano_Carrier_Board_Spec_v1.3_KO.pdf`

## 문서 성격과 번역 원칙

통합 가이드북은 공식 문서의 장·절을 추적할 수 있도록 핵심 요구사항을 한국어로 재서술한 독립 저작물입니다. 별도 캐리어보드 v1.3 번역·해설은 원문 구조와 모든 표·그림 항목을 대응시키되, 신호명과 부품번호는 원문 표기를 유지합니다. 정확한 규격, 전기적 한계와 라이선스 조건은 반드시 연결된 공식 원문을 최종 기준으로 사용하십시오.

## 지원 범위

- 개발키트: P3766
- 개발용 모듈: P3767-0005(8GB, microSD 슬롯)
- 기준 캐리어보드: P3768-0000
- 주요 헤더: J12(40핀), J14(버튼/디버그), J20/J21(CSI), J13(팬)
- 목표 워크로드: 카메라 수집, GPU 가속 Vision, VLM, 안전한 ROS 2 행동 실행, 경량 VLA

## 중요한 제한

- Jetson Orin Nano 8GB는 학습용 대형 VLA 전체를 처음부터 훈련하는 장비가 아닙니다. 주 학습은 x86_64 GPU 서버에서 하고 Jetson에서는 양자화된 추론·센서 처리·안전 제어를 수행하는 구성이 현실적입니다.
- 핫플러그가 명시되지 않은 CSI·GPIO·UART·I2C 장치는 반드시 전원을 끈 뒤 연결하십시오.
- `rm -rf`, 디스크 플래시, 파티션 변경, Secure Boot 퓨즈 작업은 대상 장치 확인 없이 실행하지 마십시오.

