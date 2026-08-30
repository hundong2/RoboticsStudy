# 번역본 안내

원문: NVIDIA, *Jetson Orin Nano Developer Kit Carrier Board Specification*, SP-11324-001 v1.3, 2024-12.

이 문서는 원문의 장·절·표·그림 순서를 따라 작성한 한국어 기술 번역본이다. 신호명, 부품번호, 커넥터 번호와 레일명은 회로도·데이터시트와 대조할 수 있도록 원문 표기를 유지했다. 전기 설계, 제품 인증 및 양산 판단에는 반드시 NVIDIA 원문과 모듈 Product Design Guide/Data Sheet를 최종 기준으로 사용한다.

## 문서 개정 이력

| 버전 | 날짜 | 변경 내용 |
|---|---|---|
| 1.0 | 2023-03-20 | 최초 발행 |
| 1.1 | 2023-05-17 | VDD_IN 주석, DisplayPort J8 핀 13, M.2 Key E J10 핀 55/57, 개발 키트 무게, 전원도와 VDD_5V_SYS 전류 표 수정 |
| 1.2 | 2024-04-12 | Figure 1-4의 J20/J21 카메라 커넥터 핀 1 위치 수정 |
| 1.3 | 2024-12-20 | 개발 키트 캐리어보드에서 Orin NX 40W MAXN_SUPER 미지원 명시, Orin Nano 메모리 대역폭에 MAXN_SUPER 수치 반영, J17 CAN 헤더 피치를 2.54 mm로 정정 |

## 핵심 안전 경고

- Jetson 모듈과 모든 외부 장치를 먼저 연결한 뒤 AC 전원을 연결한다.
- 전원이 켜진 상태에서 모듈, 카메라, M.2 또는 확장 헤더 장치를 연결하거나 분리하지 않는다.
- 작업 전 전원을 제거하고 VDD_IN 적색 LED가 꺼질 때까지 기다린다. LED가 없는 시스템은 5분간 기다린다.
- 보드는 ESD 민감 부품을 포함한다. 접지된 손목 스트랩과 정전기 방지 작업대를 사용한다.
- 개발자 키트 캐리어보드는 MODULE_ID 풀업이 제거되어 모듈 종류와 관계없이 VDD_IN에 5V만 공급한다.
- 커스텀 캐리어보드는 MODULE_ID를 사용하여 5V/19V 입력을 구분하도록 설계할 수 있다.
- Orin NX 40W(MAXN_SUPER)는 이 개발자 키트 캐리어보드에서 지원되지 않는다.

# 1. 소개

이 사양서는 Jetson Orin Nano 캐리어보드의 확장 커넥터용 모듈을 설계하는 엔지니어에게 필요한 권고사항과 지침을 제공한다. 전용 인터페이스 커넥터의 기능과 플랫폼 전원 구조도 설명한다. 캐리어보드는 Linux 환경의 소프트웨어 개발용으로 설계됐으며, 표준 커넥터를 통해 모듈 기능과 인터페이스에 접근한다. Jetson Orin Nano 시리즈 및 Jetson Orin NX 시리즈 모듈을 지원하지만, 전원·성능 모드 제한은 앞의 안전 경고를 따른다.

## 1.1 Jetson Orin Nano 모듈 기능

- 애플리케이션 프로세서: NVIDIA Orin.
- 메모리: 8GB, 128-bit LPDDR5. 대역폭은 최대 68GB/s, MAXN_SUPER에서 102GB/s.
- 저장장치: UHS-I microSD 카드 소켓.
- 네트워크: 10/100/1000BASE-T Ethernet.
- 전원 관리: 동적 전압·주파수 조절, 복수 클럭 및 전원 도메인.

## 1.2 캐리어보드 기능

- 모듈 연결: 260핀 SO-DIMM J2.
- USB: 복구 모드를 지원하는 USB-C J5, USB 3.2 Gen2x1 허브에 연결된 호스트 전용 Type-A 4포트 J6/J7.
- 유선 네트워크: LED와 선택형 PoE 헤더를 포함한 Gigabit Ethernet J15.
- 디스플레이: DisplayPort 1.2(MST 포함), eDP 1.4.
- 카메라: 22핀 0.5mm FFC 커넥터 2개. J20은 x2, J21은 x2 또는 x4 CSI 구성. 카메라 클럭·I2C·제어 제공.
- M.2 Key E J10: PCIe Gen3 x1, USB 2.0, I2S, UART, 선택형 I2C.
- M.2 Key M: J11 PCIe x4, J24 PCIe x2.
- J12 확장 헤더: 2x20, I2C 2개, SPI 2개, UART, I2S, 오디오 클럭, GPIO, PWM.
- J14 버튼 헤더: 전원, 리셋, 강제 복구, 디버그 UART, Auto-Power-On 비활성화, LED.
- 기타: J13 팬, 선택형 J3 RTC 배터리, 선택형 J17 CAN.
- 입력 전원: J16 DC 9~20V. 제공 어댑터는 19V. 선택형 PoE와 backpower 헤더 제공.
- 주요 레귤레이터: 5V GS9230 또는 A0Z2264, 3.3V GS9230 또는 A0Z2264, 1.8V·3.3V_AO GS7116S5.
- USB VBUS 스위치: AP22811AW5-7 2개. DP 3.3V 스위치: APL3552ABI-TRG.
- 개발 키트 동작 온도: 0~35°C.

## 1.3 블록도와 배치도 번역

- Figure 1-1: Jetson Orin Nano 블록도.
- Figure 1-2: 모듈 상면 외형(envelope).
- Figure 1-3: 모듈 하면 외형.
- Figure 1-4: 캐리어보드 상면 배치. J17 CAN, J14 버튼, J13 팬, J20/J21 카메라, J12 40핀, J18/J19 PoE, J6/J7 USB-A, J15 Ethernet, J8 DP, J5 USB-C, J16 DC 잭, DS1 전원 LED, J2 SO-DIMM 위치를 표시한다.
- Figure 1-5: 캐리어보드 하면 배치. J10 M.2 Key E, J11 x4 Key M, J24 x2 Key M, J3 RTC 배터리 위치를 표시한다.

| 위치 | 커넥터 설명 |
|---|---|
| J2 | Jetson 모듈용 260핀 SO-DIMM |
| J5 | USB Type-C |
| J6/J7 | 듀얼 스택 USB Type-A |
| J8 | DisplayPort |
| J10 | M.2 Key E 75핀 |
| J11/J24 | M.2 Key M 75핀, 각각 x4/x2 |
| J12 | 2x20, 2.54mm 확장 헤더 |
| J13 | 4핀, 1.25mm 팬 헤더 |
| J14 | 1x12, 2.54mm 직각 버튼 헤더 |
| J15 | 18핀 RJ45 Ethernet |
| J16 | DC 전원 잭 |
| J17 | 선택형 1x4, 2.54mm CAN 헤더 |
| J18/J19 | 선택형 PoE backpower 1x2 / PoE 1x4 |
| J20/J21 | 22핀, 0.5mm 카메라 FFC |
| DS1 | 녹색 전원 LED |

# 2. 표준 커넥터

## 2.1 USB 포트

J5는 host/device/USB Recovery를 지원하는 USB 3.2 Type-C 포트다. J6/J7은 각각 듀얼 스택 Type-A이며 host 모드만 지원한다. 각 스택의 USB VBUS는 부하 스위치를 거치며 출력 전류 제한은 3A다.

### Table 2-1. USB 3.2 Type-C J5

| 핀 | 커넥터 신호 | 모듈 신호/핀 | 용도 | 방향 |
|---|---|---|---|---|
| A1/A12 | GND_A | - | 접지 | Ground |
| A2/A3 | TX1_P/N | USBSS1_TX_P/N, G23/G22 | mux에서 나온 USB 3.2 #1 송신 1 | Output |
| A4/A9 | VBUS_A | - | USB VBUS 전원 | Power |
| A5 | CC1 | - | CC 컨트롤러의 CC1 | Output |
| A6/A7 | D1_P/N | USB0_D_P/N, F12/F13 | USB 2.0 #0 Data 1 | Bidir |
| A8 | SBU1 | - | 미연결 | - |
| A10/A11 | RX2_N/P | USBSS1_RX_P/N, C22/C23 | mux에서 온 USB 3.2 #1 수신 2 | Input |
| B1/B12 | GND_B | - | 접지 | Ground |
| B2/B3 | TX2_P/N | USBSS1_TX_P/N, G23/G22 | mux에서 나온 USB 3.2 #1 송신 2 | Output |
| B4/B9 | VBUS_A | - | USB VBUS 전원 | Power |
| B5 | CC2 | - | CC 컨트롤러의 CC2 | Output |
| B6/B7 | D2_P/N | USB0_D_P/N, F12/F13 | USB 2.0 #0 Data 2 | Bidir |
| B8 | SBU2 | - | 미연결 | - |
| B10/B11 | RX1_N/P | USBSS1_RX_P/N, C22/C23 | mux에서 온 USB 3.2 #1 수신 1 | Input |

주: SuperSpeed 모듈 핀은 커넥터에 직결되지 않고 multiplexer를 통과한다. 방향은 커넥터 기준이다.

### Table 2-2/2-3. USB Type-A J6/J7

| 커넥터 | 핀 | 모듈 신호/핀 | 용도 | 방향 |
|---|---|---|---|---|
| J6 상단 포트 #2 | 1,4,7 | VBUS/GND | 5V 및 접지 | Power/Ground |
| J6 상단 포트 #2 | 2/3 | USB1_D_N/P | USB 2.0 #2, 허브 경유 | Bidir |
| J6 상단 포트 #2 | 5/6 | USBSS0_RX_N/P, 161/163 | USB 3.1 수신 #2, 허브 경유 | Input |
| J6 상단 포트 #2 | 8/9 | USBSS0_TX_N/P, 166/168 | USB 3.1 송신 #2, 허브 경유 | Output |
| J6 하단 포트 #1 | 10,13,16 | VBUS/GND | 5V 및 접지 | Power/Ground |
| J6 하단 포트 #1 | 11/12 | USB1_D_N/P, 115/117 | USB 2.0 #1, 허브 경유 | Bidir |
| J6 하단 포트 #1 | 14/15 | USBSS0_RX_N/P, 161/163 | USB 3.1 수신 #1 | Input |
| J6 하단 포트 #1 | 17/18 | USBSS0_TX_N/P, 166/168 | USB 3.1 송신 #1 | Output |
| J7 상단 포트 #4 | 1,4,7 | VBUS/GND | 5V 및 접지 | Power/Ground |
| J7 상단 포트 #4 | 2/3 | USB1_D_N/P, 115/117 | USB 2.0 #4 | Bidir |
| J7 상단 포트 #4 | 5/6 | USBSS0_RX_N/P, 161/163 | USB 3.1 수신 #4 | Input |
| J7 상단 포트 #4 | 8/9 | USBSS0_TX_N/P, 166/168 | USB 3.1 송신 #4 | Output |
| J7 하단 포트 #3 | 10,13,16 | VBUS/GND | 5V 및 접지 | Power/Ground |
| J7 하단 포트 #3 | 11/12 | USB1_D_N/P, 115/117 | USB 2.0 #3 | Bidir |
| J7 하단 포트 #3 | 14/15 | USBSS0_RX_N/P, 161/163 | USB 3.1 수신 #3 | Input |
| J7 하단 포트 #3 | 17/18 | USBSS0_TX_N/P, 166/168 | USB 3.1 송신 #3 | Output |

## 2.2 Gigabit Ethernet J15

| 핀 | 모듈 신호/핀 | 용도 | 방향 |
|---|---|---|---|
| 1/2 | GPE_MDI0_P/N, 186/184 | MDI 0 +/- | Bidir |
| 3/6 | GPE_MDI1_P/N, 192/190 | MDI 1 +/- | Bidir |
| 4/5 | - | MCT | - |
| 7/8 | GPE_MDI2_P/N, 198/196 | MDI 2 +/- | Bidir |
| 9/10 | GPE_MDI3_P/N, 204/202 | MDI 3 +/- | Bidir |
| 11~14 | - | Power-over-Ethernet | Power |
| 15/16 | GBE_LED_LINK, 188 | 녹색 LED. 1000Mbps 링크에서 켜지고 10/100Mbps에서는 꺼짐 | Input/Output |
| 17/18 | GBE_LED_ACT, 194 | 노란색 LED. 활동 시 켜짐 | Input/Output |
| 19/20 | Shield | 실드 접지 | Ground |

## 2.3 DisplayPort J8

| 핀 | 모듈 신호/핀 | 용도 | 방향 |
|---|---|---|---|
| 1/3 | DP0_TXD0_P/N, 41/39 | DP Lane 0 +/- | Output |
| 2 | - | 접지 | Ground |
| 4/6 | DP0_TXD1_P/N, 47/45 | DP Lane 1 +/- | Output |
| 5 | - | 접지 | Ground |
| 7/9 | DP0_TXD2_P/N, 53/51 | DP Lane 2 +/- | Output |
| 8 | - | 접지 | Ground |
| 10/12 | DP0_TXD3_P/N, 59/57 | DP Lane 3 +/- | Output |
| 11 | - | 접지 | Ground |
| 13 | MODE | PI3AUX221ZTAEX를 통해 DP/HDMI 모드 선택 | Input |
| 14 | CEC_DP | 미사용, 1Mohm으로 GND 풀다운 | Unused |
| 15/17 | DP0_AUX_N/P, 90/92 | DP 보조 채널 0 -/+ | Bidir |
| 16/19 | - | 접지/전원 리턴 | Ground |
| 18 | DP0_HPD, 88 | Hot Plug Detect | Input |
| 20 | +3.3V | 커넥터 전원 | Power |

## 2.4 M.2 Key E J10

J10은 단면 Key E 모듈만 지원한다. 기본으로 PCIe x1, USB 2.0, UART, I2S를 제공한다. I2C2를 핀 58/60에 연결하려면 기본 미실장인 R106/R107에 0ohm 저항을 실장해야 한다. 일부 M.2 카드가 같은 I2C 버스 장치와 충돌할 수 있다.

| 핀 | 신호 | 모듈 핀 | 번역된 용도/방향 |
|---|---|---|---|
| 1,18,33,39,45,51,57,63,69,75 | GND | - | 접지 |
| 2,4,72,74 | Main 3.3V | - | 주 3.3V 전원 |
| 3/5 | USB2_D_P/N | 123/121 | USB 2.0 데이터, 양방향 |
| 8 | I2S1_CLK | 226 | I2S #1 클럭, 1.8V 양방향 |
| 10 | I2S1_FS | 224 | I2S #1 LR 클럭, 1.8V 양방향 |
| 12 | I2S1_DIN | 222 | I2S #1 입력, 1.8V |
| 14 | I2S1_DOUT | 220 | I2S #1 출력, 1.8V |
| 20 | GPIO02 | 124 | Bluetooth #2 Wake AP, 3.3V 입력 |
| 22 | UART0_RXD | 101 | UART #0 수신, 1.8V 입력 |
| 32 | UART0_TXD | 99 | UART #0 송신, 1.8V 출력 |
| 34 | UART0_CTS* | 105 | UART #0 CTS, 1.8V 입력 |
| 35/37 | PEX1_TX0_P/N | 174/172 | PCIe #1 Lane 0 송신, 출력 |
| 36 | UART0_RTS* | 103 | UART #0 RTS, 1.8V 출력 |
| 41/43 | PEX1_RX0_P/N | 169/167 | PCIe #1 Lane 0 수신, 입력 |
| 47/49 | PEX1_CLK_P/N | 175/173 | PCIe #1 기준 클럭, 출력 |
| 50 | CLK_32K_OUT | 210 | 32kHz suspend clock, 3.3V 출력 |
| 52 | PEX0_RST* | 183 | PCIe #0 reset, 3.3V 출력 |
| 53 | PEX1_CLKREQ* | 182 | PCIe #1 clock request, 3.3V 양방향 |
| 54 | GPIO3 | 126 | Bluetooth enable, 3.3V 출력 |
| 55 | PEX_WAKE* | 179 | PCIe wake, 3.3V 입력 |
| 56 | GPIO5 | 128 | Wi-Fi disable, 3.3V 출력 |
| 58/60 | I2C2_SDA/SCL | 234/232 | 선택형 I2C #2, 1.8V open-drain 양방향 |
| 62 | GPIO10 | 212 | Alert, 1.8V 입력 |
| 6,9,16,17,38,40,42,44,46,48,59,61,64~68,70,71,73 | - | - | 미사용 또는 key 위치 |

## 2.5 M.2 Key M J11/J24

J11은 PCIe x4, J24는 PCIe x2이며 커넥터는 Gen4 배선을 제공한다. Orin Nano 모듈은 최대 Gen3, Orin NX는 최대 Gen4를 지원한다. 개발자 키트는 단면 M.2 Key M 모듈만 지원한다.

### Table 2-7. J11 활성 핀

| 핀 | 신호/모듈 핀 | 용도 |
|---|---|---|
| 2,4,14,16,72,74 | 3.3V | 주 전원 |
| 1,3,9,15,21,27,33,39,45,51,57,73 | GND | 접지 |
| 5/7 | PCIE0_RX3_N/P, 155/157 | Lane 3 수신 |
| 11/13 | PCIE0_TX3_N/P, 154/156 | Lane 3 송신 |
| 17/19 | PCIE0_RX2_N/P, 149/151 | Lane 2 수신 |
| 23/25 | PCIE0_TX2_N/P, 148/150 | Lane 2 송신 |
| 29/31 | PCIE0_RX1_N/P, 137/139 | Lane 1 수신 |
| 35/37 | PCIE0_TX1_N/P, 140/142 | Lane 1 송신 |
| 41/43 | PCIE0_RX0_N/P, 131/133 | Lane 0 수신 |
| 47/49 | PCIE0_TX0_N/P, 134/136 | Lane 0 송신 |
| 44 | GPIO10, 212 | M.2 Alert, 1.8V 입력 |
| 50 | PEX0_RST*, 181 | PCIe #0 reset, 3.3V 출력 |
| 52 | PEX0_CLKREQ*, 180 | PCIe #0 clock request, 3.3V 입력 |
| 53/55 | PCIE0_CLK_N/P, 160/162 | 기준 클럭 출력 |
| 54 | PEX_WAKE*, 179 | PCIe wake, 3.3V에서 1.8V로 level shift된 입력 |
| 68 | 32kHz Suspend Clock | 3.3V 출력 |
| 6,8,10,12,18,20,22,24,26,28,30,32,34,36,38,40,42,46,48,56,58~67,69~71,75 | - | 미사용/key/no pin |

### Table 2-8. J24 활성 핀

| 핀 | 신호/모듈 핀 | 용도 |
|---|---|---|
| 2,4,14,16,72,74 | 3.3V | 주 전원 |
| 1,3,9,15,21,27,33,39,45,51,57,73 | GND | 접지 |
| 29/31 | PCIE2_RX1_N/P, 58/60 | Lane 1 수신 |
| 35/37 | PCIE2_TX1_N/P, 64/66 | Lane 1 송신 |
| 41/43 | PCIE2_RX0_N/P, 40/42 | Lane 0 수신 |
| 47/49 | PCIE2_TX0_N/P, 46/48 | Lane 0 송신 |
| 44 | GPIO10, 212 | M.2 Alert, 1.8V 입력 |
| 50 | PEX2_RST*, 219 | PCIe reset, 3.3V 출력 |
| 52 | PEX2_CLKREQ*, 221 | PCIe clock request, 3.3V 입력 |
| 53/55 | PCIE2_CLK_N/P, 52/54 | 기준 클럭 출력 |
| 54 | PEX_WAKE*, 179 | PCIe wake 입력 |
| 68 | 32kHz Suspend Clock | 3.3V 출력 |
| 그 밖의 핀 | - | 미사용/key/no pin. 원문 Table 2-8의 번호 체계를 유지한다. |

# 3. 커스텀 확장 인터페이스

## 3.1 Jetson 모듈 커넥터 J2

260핀, 1.27mm SO-DIMM이며 TE Connectivity 2309413-1을 사용한다. 모듈 edge finger와 연결된다. 전체 핀아웃은 Jetson Orin Nano Product Design Guide를 참조한다.

## 3.2 카메라 J20/J21

보드 커넥터는 Molex Japan 54548-2272, 22핀 0.5mm pitch다. J20은 x2 CSI, J21은 x2 또는 x4 CSI를 지원하며 CAM_I2C, MCLK, PWDN, 3.3V를 제공한다.

### Table 3-1. J20 Camera #0

| 핀 | 신호/모듈 핀 | 용도 | 방향 |
|---|---|---|---|
| 1 | 3.3V | 카메라 전원 | Power |
| 2/3 | CAM_I2C_SDA/SCL, 215/213 | mux 첫 번째 출력. 모듈 2.2kohm, 카메라 측 1kohm 풀업 | Bidir/Output, 3.3V |
| 4,7,10,13,16,19,22 | GND | 접지 | Ground |
| 5 | CAM0_MCLK, 116 | Camera #0 주 클럭 | Output, 1.8V |
| 6 | CAM0_PWDN, 114 | Camera #0 power-down | Output, 1.8V |
| 8/9 | CSI0_D1_P/N, 18/16 | CSI 0 Data 1 | Input |
| 11/12 | CSI0_D0_P/N, 6/4 | CSI 0 Data 0 | Input |
| 14/15 | CSI1_CLK_P/N, 11/9 | CSI 0 Clock | Input |
| 17/18 | CSI1_D1_P/N, 17/15 | CSI 1 Data 1 | Input |
| 20/21 | CSI1_D0_P/N, 5/3 | CSI 1 Data 0 | Input |

### Table 3-2. J21 Camera #1

| 핀 | 신호/모듈 핀 | 용도 | 방향 |
|---|---|---|---|
| 1 | 3.3V | 카메라 전원 | Power |
| 2/3 | CAM_I2C_SDA/SCL, 215/213 | mux 두 번째 출력. 모듈 2.2kohm, 카메라 측 1kohm 풀업 | Bidir/Output, 3.3V |
| 4,7,10,13,16,19,22 | GND | 접지 | Ground |
| 5 | CAM1_MCLK, 122 | 원문 표기 Camera #0 Primary Clock | Output, 1.8V |
| 6 | CAM1_PWDN, 120 | 원문 표기 Camera #0 Power-down | Output, 1.8V |
| 8/9 | CSI3_D1_P/N, 35/33 | CSI 3 Data 1 | Input |
| 11/12 | CSI3_D0_P/N, 23/21 | CSI 3 Data 0 | Input |
| 14/15 | CSI2_CLK_P/N, 30/28 | CSI 2 Clock | Input |
| 17/18 | CSI2_D1_P/N, 36/34 | CSI 2 Data 1 | Input |
| 20/21 | CSI2_D0_P/N, 24/22 | CSI 2 Data 0 | Input |

## 3.3 J12 40핀 확장 헤더

J12는 Astron Technology 27-0169H-220-1G-H, 2x20, 2.54mm다. 모든 신호 레벨은 3.3V다. I2S/I2C/SPI/UART/오디오 클럭 핀은 GPIO로 재구성할 수 있다. I2C를 제외한 외부 pull-up/down은 50kohm보다 큰 약한 저항을 사용한다.

| 핀 | 기본 신호 | 모듈 핀 | 기본 용도/대체 기능 | 방향·기본 pull |
|---|---|---|---|---|
| 1 | 3.3V | - | 주 3.3V, 핀당 1A | Power |
| 2 | 5V | - | 주 5V, 핀당 1A | Power |
| 3 | I2C1_SDA | 191 | I2C #1 data | OD Bidir, 2.2kohm PU |
| 4 | 5V | - | 주 5V, 핀당 1A | Power |
| 5 | I2C1_SCL | 189 | I2C #1 clock | OD Bidir, 2.2kohm PU |
| 6 | GND | - | 접지 | Ground |
| 7 | GPIO09 | 211 | Audio primary clock | Bidir/Output, pd |
| 8 | UART1_TXD | 203 | UART #1 transmit/GPIO | Output/Bidir, pd |
| 9 | GND | - | 접지 | Ground |
| 10 | UART1_RXD | 205 | UART #1 receive/GPIO | Input/Bidir, pd |
| 11 | UART1_RTS* | 207 | UART RTS/GPIO | Bidir/Output, pd |
| 12 | I2S0_SCLK | 199 | I2S #0 clock/GPIO | Bidir, pd |
| 13 | SPI1_SCK | 106 | SPI #1 clock/GPIO | Bidir/Output, pd |
| 14 | GND | - | 접지 | Ground |
| 15 | GPIO12 | 218 | PWM1/GPIO | Bidir, z |
| 16 | SPI1_CS1* | 112 | SPI #1 CS1/GPIO | Bidir/Output, z |
| 17 | 3.3V | - | 주 3.3V, 핀당 1A | Power |
| 18 | SPI1_CS0* | 110 | SPI #1 CS0/GPIO | Bidir/Output, z |
| 19 | SPI0_MOSI | 89 | SPI #0 MOSI/GPIO | Bidir/Output, pd |
| 20 | GND | - | 접지 | Ground |
| 21 | SPI0_MISO | 93 | SPI #0 MISO/GPIO | Bidir/Input, pd |
| 22 | SPI1_MISO | 108 | SPI #1 MISO/GPIO | Bidir/Input, pd |
| 23 | SPI0_SCK | 91 | SPI #0 clock/GPIO | Bidir/Output, pd |
| 24 | SPI0_CS0* | 95 | SPI #0 CS0/GPIO | Bidir/Output, z |
| 25 | GND | - | 접지 | Ground |
| 26 | SPI0_CS1* | 97 | SPI #0 CS1/GPIO | Bidir/Output, pu |
| 27 | I2C0_SDA | 187 | I2C #0 data/GPIO | OD Bidir, 1.5kohm PU |
| 28 | I2C0_SCL | 185 | I2C #0 clock/GPIO | OD Bidir, 1.5kohm PU |
| 29 | GPIO01 | 118 | General purpose clock #0 | Bidir/Output, pd |
| 30 | GND | - | 접지 | Ground |
| 31 | GPIO11 | 216 | General purpose clock #1 | Bidir/Output, pd |
| 32 | GPIO07 | 206 | PWM7/GPIO | Bidir/Output, z |
| 33 | GPIO13 | 228 | PWM/GPIO | Bidir/Output, z |
| 34 | GND | - | 접지 | Ground |
| 35 | I2S0_FS | 197 | I2S #0 frame select/GPIO | Bidir, pd |
| 36 | UART1_CTS* | 209 | UART CTS/GPIO | Bidir/Input, pd |
| 37 | SPI1_MOSI | 104 | SPI #1 MOSI/GPIO | Bidir/Output, pd |
| 38 | I2S0_DIN | 195 | I2S #0 data in/GPIO | Bidir/Input, pd |
| 39 | GND | - | 접지 | Ground |
| 40 | I2S0_DOUT | 193 | I2S #0 data out/GPIO | Bidir/Output, pd |

J12 주석 번역:

1. 전원 전류는 전원 핀 하나당 허용치다.
2. I2C 핀은 SoC에 직접 연결된 open-drain이며, 데이터시트 VOL을 만족하는 최대 구동은 +/-2mA다.
3. 그 밖의 신호는 TI TXB0108 레벨 변환기를 거친다. 출력 구동이 매우 약하여 양방향 지원 시 외부 장치가 신호를 덮어쓸 수 있다.
4. 방향은 헤더 기준이다. 두 방향이 있으면 첫 번째가 GPIO 등 주 기능, 두 번째가 대체 기능이다.
5. 표의 입력/출력은 SPI/I2S 등의 일반적 특수 기능 사용을 기준으로 한다. GPIO 구성 시 양방향이다.
6. 모든 J12 신호는 3.3V 레벨이다.

## 3.4 J14 버튼 헤더

| 핀 | 신호 | 번역된 용도 | 방향/레벨 |
|---|---|---|---|
| 1 | PC_LED- | 시스템 sleep/wake LED cathode. sleep일 때 꺼짐 | Input, 5V |
| 2 | PC_LED+ | LED anode | Output |
| 3 | UART2_RXD(DEBUG) | 디버그 UART 수신 | Input, 3.3V |
| 4 | UART2_TXD(DEBUG) | 디버그 UART 송신 | Output, 3.3V |
| 5 | AC OK | 5-6을 연결하면 Auto-Power-On 비활성화 | Input, 3.3V |
| 6 | Auto Power-on disable | GND로 pull됨. 핀 5와 연결 | N/A |
| 7 | GND | 접지 | Ground |
| 8 | SYS_RESET* | 7-8을 순간 연결하면 리셋 | Input, 1.8V |
| 9 | GND | 접지 | Ground |
| 10 | FORCE_RECOVERY* | 전원 투입 중 9-10을 연결하면 USB Force Recovery | Input, 1.8V |
| 11 | GND | 접지 | Ground |
| 12 | SLEEP/WAKE* | Auto-Power-On 비활성 상태에서 11-12를 순간 연결하면 전원 켜짐 | Input, 5V |

전원 버튼은 무전압 순간동작(NO) 스위치 두 선을 J14 핀 11(GND)과 핀 12(SLEEP/WAKE*)에 연결한다. 핀 5-6 점퍼로 Auto-Power-On을 비활성화했을 때 사용한다. 외부 전압을 인가하지 않는다.

## 3.5 J17 선택형 CAN

| 핀 | 신호/모듈 핀 | 용도 | 방향 |
|---|---|---|---|
| 1 | CAN_TX, 145 | CAN 송신 | Output, 3.3V |
| 2 | CAN_RX, 143 | CAN 수신 | Input, 3.3V |
| 3 | GND | 접지 | Ground |
| 4 | 3.3V | 주 3.3V | Power |

J17은 1x4, 2.54mm footprint이며 보통 미실장이다. CAN_H/CAN_L이 아니라 SoC 논리 신호이므로 외부 CAN transceiver가 필요하다.

## 3.6 J13 팬

| 핀 | 신호/모듈 핀 | 용도 | 방향 |
|---|---|---|---|
| 1 | GND | 접지 | Ground |
| 2 | 5V | 팬 전원 | Power |
| 3 | GPIO08, 208 | tachometer | Input, 5V |
| 4 | GPIO14, 230 | PWM | Output, 5V |

커넥터는 Singatron 2WBA2542WVC-F-04PNLBT1N00G, 4핀 1.25mm다.

## 3.7 J3 RTC 백업 배터리

Wieson AC2651-0011-003-HH, 2핀 1.25mm다. 핀 1은 PMIC_BBAT(모듈 핀 235), 핀 2는 GND다. Figure 1-5는 CR1225 선택형 소켓 위치를 표시한다.

## 3.8 J16 DC 전원 잭

- 부품: Singatron 2DC-0005D206F.
- barrel 길이 9.5mm, 외경 5.5mm, 내경/핀 2.5mm.
- center-positive.
- 잭 최대 지원 전류 3.5A.
- 핀 1은 9~20V DC 입력, 핀 2/3은 GND.

## 3.9 J19 PoE와 J18 backpower

J19는 RJ45 PoE VC1~VC4를 내보내는 1x4, 2.54mm 헤더다. PoE 38~60V를 캐리어보드 입력 범위 5~20V로 변환하는 외부 컨버터가 필요하다. 컨버터 출력은 J18 1x2 backpower 헤더로 들어가며 최대 3A다.

| 헤더/핀 | 용도 |
|---|---|
| J19-1/2/3/4 | RJ45 PoE VC1/VC2/VC3/VC4 전원 |
| J18-1 | 주 DC 입력 9~20V, 최대 3A |
| J18-2 | GND |

# 4. 기구 사양

- 개발 키트 무게: 0.175kg.
- Figure 4-1 캐리어보드: 100.00 +/-0.13mm x 79.00 +/-0.13mm, 상면 최대 높이 16.70mm, 하면 최대 높이 4.30mm, PCB 두께 1.57 +/-0.16mm.
- Figure 4-2 개발 키트 전체: 103.00 +/-0.20mm x 90.50 +/-0.20mm x 34.77 +/-1.09mm.
- 실제 인클로저·스탠드오프·FFC·팬 간섭 설계에는 원문의 치수 도면을 직접 사용한다.

# 5. 인터페이스 전원

Figure 5-1의 핵심은 MODULE_ID pull-up이 제거되어 VDD_5V_SYS 5V 모드만 지원된다는 점이다. DC 잭은 power mux와 GS9230 DC-DC를 통해 VDD_5V_SYS를 만들고 모듈, USB 스위치, 팬에 공급한다. 별도 GS9230은 VDD_3V3_SYS, GS7116S5는 3V3_AO, AP2127K는 VDD_1V8을 만든다.

## Table 5-1. 전원 레일 할당

| 레일 | 용도 | 전압 | 공급원 | 입력/Enable |
|---|---|---:|---|---|
| DC_IN | DC 어댑터 주 입력 | 19.5V | AONR21357 power mux | DC adapter |
| VDD_5V_SYS | 주 5V | 5.0V | GS9230NVTQ-R | power FET 뒤 VDD_CVB |
| VDD_3V3_SYS | 주 3.3V | 3.3V | GS9230NVTQ-R | VDD_CVB / SYS_RESET_IN* |
| 3V3_AO | 버튼 MCU always-on | 3.3V | GS7116S5-ADJ-R LDO | VDD_5V_SYS / power good |
| VDD_1V8 | 주 1.8V | 1.8V | AP2127K-1.8TRG1 | VDD_3V3_SYS / 3.3V_IO_PG |
| VDD_AV10_HUB | USB hub regulator | 1.1V | GS7303ACTD-R | VDD_5V_SYS / VDD_3V3_SYS good |
| USBC_VBUS | USB-C VBUS | 5.0V | AP22811AW5-7 | VDD_5V_SYS / CC controller ID |
| USB_VBUS_A/B | 듀얼 스택 Type-A VBUS | 5.0V | AP22811AW5-7 | VDD_5V_SYS / USB hub |
| VDD_3V3_DP | DP 커넥터 레일 | 원문 Usage 열 5.0V, 레일명상 3.3V | GS7612S5MC-R | VDD_3V3_SYS / power good |

원문 Table 5-1의 VDD_3V3_DP Usage 값은 5.0으로 표시되어 있으므로 설계 시 회로도와 최신 사양서를 다시 대조한다.

## Table 5-2. 레일별 최대 전류

| 레일 | 전압 | 최대 전류 |
|---|---:|---:|
| DCJ_IN | 19.0V | 4.2A |
| VDD_5V_SYS | 5.0V | 7.8A |
| VDD_3V3_SYS | 3.3V | 5.4A |
| VDD_1V8 | 1.8V | 0.0002A |
| 3V3_AO | 3.3V | 0.200A |

## Table 5-3. 커넥터별 공급 전류

| 레일 | 커넥터 | 전압 | 최대 전류 |
|---|---|---:|---:|
| VDD_5V_SYS | SO-DIMM VDD_IN | 5.0V | 5.0A |
| VDD_5V_SYS | J12 40핀 | 5.0V | 0.5A |
| VDD_5V_SYS | J13 팬 | 5.0V | 0.15A |
| VDD_3V3_DP | J8 DP | 3.3V | 0.5A |
| USBC_VBUS | J5 USB-C | 5.0V | 0.5A |
| USB_VBUS_A/B | USB-A 4포트 | 5.0V | 포트/표 기준 0.5A |
| VDD_3V3_SYS | J12 40핀 | 3.3V | 0.1A |
| VDD_3V3_SYS | J10 M.2 Key E | 3.3V | 0.8A |
| VDD_3V3_SYS | J11/J24 M.2 Key M | 3.3V | 합계 2.1A |
| VDD_3V3_SYS | J20/J21 카메라 | 3.3V | 합계 0.26A |

개별 핀 허용치와 전체 레일 허용치는 동시에 만족해야 한다. 예를 들어 J12 5V 핀 자체 표기는 핀당 1A지만, Table 5-3의 J12 5V 커넥터 예산은 0.5A이므로 더 작은 전체 예산을 우선한다.

# 부록 A. 원문 표·그림 대응표

| 원문 항목 | 번역 위치 |
|---|---|
| Figure 1-1~1-5 | 1.3 블록도와 배치도 번역 |
| Table 2-1~2-3 | 2.1 USB 포트 |
| Table 2-4 | 2.2 Ethernet |
| Table 2-5 | 2.3 DisplayPort |
| Table 2-6 | 2.4 M.2 Key E |
| Table 2-7~2-8 | 2.5 M.2 Key M |
| Table 3-1~3-2 | 3.2 카메라 |
| Figure 3-1 / Table 3-3 | 3.3 J12 |
| Table 3-4 | 3.4 J14 |
| Table 3-5~3-10 / Figure 3-2~3-3 | 3.5~3.9 |
| Figure 4-1~4-2 | 4 기구 사양 |
| Figure 5-1 / Table 5-1~5-3 | 5 인터페이스 전원 |

# 부록 B. 설계 검토 체크리스트

- [ ] 개발 키트에서는 VDD_IN 5V 모드만 사용했는가.
- [ ] Orin NX 40W MAXN_SUPER를 개발 키트 캐리어에 적용하지 않았는가.
- [ ] J14 버튼은 외부 전압 없이 접점만 단락하는가.
- [ ] J12 신호를 5V 로직에 직접 연결하지 않았는가.
- [ ] J17에 CAN transceiver를 추가했는가.
- [ ] M.2 단면 모듈과 PCIe 세대 제한을 확인했는가.
- [ ] 카메라 FFC 방향, I2C mux, pull-up, 1.8V 제어 신호를 확인했는가.
- [ ] J16 barrel 규격과 center-positive 극성을 확인했는가.
- [ ] PoE 사용 시 38~60V를 5~20V로 변환하고 J18 3A 제한을 지켰는가.
- [ ] 커넥터별 전류와 전체 레일 전류 제한을 모두 만족하는가.
- [ ] 전원 제거와 ESD 절차 후에만 모듈을 연결·분리하는가.

# 부록 C. NVIDIA 고지 전체 번역

## 고지

이 문서는 정보 제공 목적으로만 제공되며 제품의 특정 기능, 상태 또는 품질에 대한 보증으로 간주해서는 안 된다. NVIDIA Corporation(이하 NVIDIA)은 이 문서에 포함된 정보의 정확성 또는 완전성에 관하여 명시적이거나 묵시적인 어떠한 진술이나 보증도 하지 않으며, 문서에 포함된 오류에 대하여 책임을 지지 않는다. NVIDIA는 이 정보의 결과 또는 사용, 또는 그 사용으로 인해 발생할 수 있는 제3자의 특허나 기타 권리 침해에 대해 책임을 지지 않는다. 이 문서는 아래에서 정의하는 자료, 코드 또는 기능을 개발·출시·제공하겠다는 약속이 아니다.

NVIDIA는 예고 없이 언제든지 이 문서를 정정·수정·개선하거나 기타 변경을 할 권리를 가진다.

고객은 주문 전에 최신 관련 정보를 입수하고 그 정보가 최신이며 완전한지 확인해야 한다. NVIDIA 제품은 NVIDIA와 고객의 권한 있는 대표자가 서명한 개별 판매 계약에서 달리 합의하지 않는 한, 주문 승인 시 제공되는 NVIDIA 표준 판매 조건에 따라 판매된다. NVIDIA는 이 문서에서 언급된 NVIDIA 제품 구매에 고객의 일반 거래 조건을 적용하는 것에 명시적으로 반대한다. 이 문서에 의해 직·간접적인 계약상 의무가 형성되지 않는다.

NVIDIA가 서면으로 명시적으로 합의하지 않는 한 NVIDIA 제품은 의료, 군사, 항공기, 우주 또는 생명유지 장비, NVIDIA 제품의 고장이나 오작동으로 인명 피해·사망·재산 피해·환경 피해가 합리적으로 예상되는 응용 분야에 적합하도록 설계·승인·보증되지 않았다. NVIDIA는 그러한 장비 또는 응용 분야에 NVIDIA 제품을 포함하거나 사용하는 데 대해 책임을 지지 않으며, 그러한 포함 또는 사용에 따른 위험은 고객이 부담한다.

NVIDIA는 이 문서를 기반으로 한 제품이 지정된 용도에 적합하다는 어떠한 진술이나 보증도 하지 않는다. NVIDIA가 각 제품의 모든 매개변수를 반드시 시험하는 것은 아니다. 이 문서의 정보가 고객의 계획된 응용 분야에 적용 가능한지 평가·판단하고, 제품이 해당 응용 분야에 적합한지 확인하며, 응용 프로그램 또는 제품의 결함을 방지하는 데 필요한 시험을 수행하는 것은 전적으로 고객의 책임이다. 고객 제품 설계의 취약점은 NVIDIA 제품의 품질과 신뢰성에 영향을 줄 수 있으며 이 문서에 없는 추가 또는 다른 조건·요구사항을 발생시킬 수 있다. NVIDIA는 다음에 근거하거나 귀속되는 결함·손상·비용·문제에 대해 책임을 지지 않는다. (i) 이 문서에 반하는 방식으로 NVIDIA 제품을 사용하는 경우, (ii) 고객의 제품 설계.

이 문서를 통해 NVIDIA의 특허권·저작권 또는 기타 지식재산권에 대한 명시적 또는 묵시적 라이선스가 부여되지 않는다. NVIDIA가 게시한 제3자 제품 또는 서비스 정보는 해당 제품·서비스를 사용할 라이선스나 그에 대한 보증·추천을 의미하지 않는다. 그러한 정보를 사용하려면 제3자의 특허 또는 기타 지식재산권에 따른 라이선스나 NVIDIA의 특허 또는 기타 지식재산권에 따른 라이선스가 필요할 수 있다.

이 문서의 정보는 NVIDIA가 사전에 서면으로 승인한 경우에만 복제할 수 있다. 복제 시 정보를 변경하지 않아야 하고, 모든 적용 가능한 수출 법규를 완전히 준수해야 하며, 관련된 모든 조건·제한·고지를 함께 포함해야 한다.

이 문서와 모든 NVIDIA 설계 사양, 레퍼런스 보드, 파일, 도면, 진단 자료, 목록 및 기타 문서(통칭 또는 개별적으로 자료)는 현 상태 그대로 제공된다. NVIDIA는 자료에 대하여 명시적·묵시적·법정 또는 기타 어떠한 보증도 하지 않으며, 비침해성·상품성·특정 목적 적합성에 대한 모든 묵시적 보증을 명시적으로 부인한다. 법이 금지하지 않는 범위에서 NVIDIA는 이 문서의 사용으로 인해 발생하는 직접·간접·특별·부수·징벌·결과적 손해를 포함한 어떠한 손해에 대해서도, 그 원인이나 책임 이론에 관계없이 책임을 지지 않는다. 이는 NVIDIA가 그러한 손해 가능성을 통지받은 경우에도 같다. 고객이 어떠한 이유로 손해를 입더라도 이 문서에서 설명하는 제품에 대한 NVIDIA의 총 누적 책임은 해당 제품의 판매 조건에 따른 한도로 제한된다.

## 상표

NVIDIA, NVIDIA 로고, Jetson, Jetson Orin Nano 및 NVIDIA Orin은 미국 및 기타 국가에서 NVIDIA Corporation의 상표 또는 등록상표다. 그 밖의 회사명과 제품명은 해당 회사의 상표일 수 있다.

## VESA DisplayPort

DisplayPort, DisplayPort Compliance Logo, Dual-mode Sources용 DisplayPort Compliance Logo 및 Active Cables용 DisplayPort Compliance Logo는 미국 및 기타 국가에서 Video Electronics Standards Association이 소유한 상표다.

## HDMI

HDMI, HDMI 로고 및 High-Definition Multimedia Interface는 HDMI Licensing LLC의 상표 또는 등록상표다.

## 저작권

Copyright 2023 NVIDIA Corporation. All rights reserved. NVIDIA Corporation, 2788 San Tomas Expressway, Santa Clara, CA 95051. http://www.nvidia.com

## 번역 검증 메모

- 원문 PDF: 38페이지, Letter, SP-11324-001_v1.3, 2024-12.
- 원문의 오탈자 또는 모호한 값은 임의 수정하지 않고 별도 주석으로 표시했다.
- `Output/Input/Bidir` 방향은 각각 커넥터로 나감/커넥터에서 들어옴/양방향이라는 원문 기준을 따른다.
- `pu`, `pd`, `z`는 pull-up, pull-down, high-impedance를 뜻한다.
