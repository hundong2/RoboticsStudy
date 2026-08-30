# Jetson 저속 인터페이스 기초와 C/C++ 실습

이 폴더는 Jetson Orin Nano Super Developer Kit(P3766/P3768)의 J12·J17 핀을 이용해 I2C, UART, SPI, GPIO, PWM, CAN을 안전하게 익히는 실무형 실습 자료다. 기준 환경은 JetPack 7.2.1 / Jetson Linux 39.2.1이다.

## 가장 중요한 안전 규칙

- J12 신호는 **3.3V 로직**이다. 5V TTL 신호를 GPIO·UART·SPI·I2C에 직접 넣지 않는다.
- 전원·GND·신호 배선은 Jetson과 주변 장치의 전원을 모두 끈 상태에서 한다.
- 모터, 릴레이, 솔레노이드, 고휘도 LED를 GPIO에서 직접 구동하지 않는다. 드라이버와 역기전력 보호 회로를 사용한다.
- J17의 `CAN_TX/CAN_RX`는 `CAN_H/CAN_L`이 아니다. 반드시 3.3V CAN 트랜시버를 거친다.
- 물리 핀 번호, SoC 핀 번호, GPIO line offset, Linux 장치 번호는 서로 다른 식별자다. 실행 전에 현재 보드에서 확인한다.
- I2C 주소 스캔은 동작 중인 전원·클럭·PMIC 버스에서 위험할 수 있다. **J12에 연결한 외부 실습 버스만** 대상으로 한다.

## 권장 학습 순서

1. [공통 기초와 계측](docs/00_공통_기초와_안전.md)
2. [I2C](docs/01_I2C.md)
3. [UART](docs/02_UART.md)
4. [SPI](docs/03_SPI.md)
5. [GPIO와 PWM](docs/04_GPIO_PWM.md)
6. [CAN과 SocketCAN](docs/05_CAN.md)
7. [I2S 및 기타 신호](docs/06_I2S와_기타.md)
8. [문제 해결 체크리스트](docs/07_문제해결.md)

## J12 빠른 참조

| 인터페이스 | 물리 핀 | 신호 | 실습 연결 |
|---|---:|---|---|
| I2C #1 | 3 / 5 | SDA / SCL | 3.3V 센서와 공통 GND |
| I2C #0 | 27 / 28 | SDA / SCL | HAT ID 계열과 충돌 여부 확인 |
| UART #1 | 8 / 10 | Jetson TX / RX | 루프백은 8-10 연결 |
| UART 흐름 제어 | 11 / 36 | RTS* / CTS* | 필요할 때만 교차 연결 |
| SPI #0 | 19 / 21 / 23 / 24,26 | MOSI / MISO / SCK / CS0*,CS1* | 루프백은 19-21 연결 |
| SPI #1 | 37 / 22 / 13 / 18,16 | MOSI / MISO / SCK / CS0*,CS1* | 장치 데이터시트 기준 |
| PWM 후보 | 15 / 32 / 33 | PWM1 / PWM7 / PWM | Jetson-IO 설정 후 계측 |
| I2S #0 | 12 / 35 / 38 / 40 | SCLK / FS / DIN / DOUT | 3.3V 호환 오디오 장치 |
| 전원 | 1,17 / 2,4 | 3.3V / 5V | 신호 핀과 혼동 금지 |
| 접지 | 6,9,14,20,25,30,34,39 | GND | 장치와 반드시 공통 접지 |

J17 CAN은 캐리어보드에서 선택 실장되는 1x4 헤더다. 핀은 `1=CAN_TX`, `2=CAN_RX`, `3=GND`, `4=3.3V`이며 보드의 실제 실장 상태를 먼저 확인한다.

## 빌드

Jetson에서 다음 패키지를 설치한다.

```bash
sudo apt update
sudo apt install build-essential cmake i2c-tools gpiod can-utils
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

실행 파일은 `build/bin/`에 생성된다. 하드웨어 접근 권한은 배포 환경에 맞는 `udev` 규칙과 그룹으로 부여하는 것이 원칙이다. 초기 확인을 위해 `sudo`가 필요할 수 있지만, 코드 자체를 setuid로 설치하지 않는다.

## 예제 목록

| 실행 파일 | 언어 | Linux API | 목적 |
|---|---|---|---|
| `i2c_read` | C++17 | `i2c-dev`, `I2C_RDWR` | 8비트 레지스터 주소 장치의 반복 시작 읽기 |
| `uart_loopback` | C11 | `termios`, `poll` | UART TX-RX 물리 루프백 |
| `spi_loopback` | C11 | `spidev` | SPI MOSI-MISO 전이중 루프백 |
| `gpio_v2_toggle` | C11 | GPIO character device v2 | GPIO line 출력 토글 |
| `can_socketcan` | C++17 | SocketCAN | CAN/CAN FD 송수신 |
| `pwm_sysfs` | C++17 | Linux PWM sysfs | PWM 주파수·duty 출력 후 안전 정지 |

예제는 장치 노드·GPIO offset을 자동으로 추측하지 않는다. 잘못된 자동 선택보다 사용자가 `gpioinfo`, `i2cdetect -l`, `/proc/device-tree`, Jetson-IO 결과를 확인해 명시하는 방식이 안전하다.

## 공식 기준

- NVIDIA Jetson Linux r39.2.1 Developer Guide, Configuring the Jetson Expansion Headers
- NVIDIA Jetson Linux r39.2 계열 Developer Guide, Controller Area Network
- Jetson Orin Nano Developer Kit Carrier Board Specification SP-11324-001 v1.3
- Linux kernel userspace API: GPIO character device, i2c-dev, spidev, tty, SocketCAN, PWM

