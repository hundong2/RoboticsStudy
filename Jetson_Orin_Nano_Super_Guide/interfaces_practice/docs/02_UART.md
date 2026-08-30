# 2. UART 기초와 물리 루프백

## 2.1 핵심 개념

UART는 별도 클럭선 없이 TX와 RX로 비트를 직렬화한다. 양쪽의 baud rate, data bits, parity, stop bits가 같아야 한다. `115200 8N1`은 115200 baud, 8 data bits, no parity, 1 stop bit다.

```text
Jetson TX (J12-8) ─────> device RX
Jetson RX (J12-10) <──── device TX
Jetson GND          ───── device GND
```

TTL UART와 RS-232, RS-485는 전기 규격이 다르다. RS-232 커넥터를 J12에 직접 연결하지 않는다.

## 2.2 루프백 배선

전원을 끄고 J12 핀 8(TX)과 10(RX)을 점퍼로 연결한다. 외부 장치는 연결하지 않는다. 하드웨어 흐름 제어는 사용하지 않는다.

J14 핀 3/4는 부팅 로그용 디버그 UART이며 J12 UART 실습과 용도가 다르다. 시스템 콘솔을 점유한 UART를 애플리케이션 데이터 채널로 쓰지 않는다.

## 2.3 장치 찾기와 실행

```bash
ls -l /dev/ttyTHS* /dev/ttyUSB* 2>/dev/null
grep -R "serial@" /proc/device-tree/aliases 2>/dev/null
sudo /opt/nvidia/jetson-io/config-by-function.py -l enabled
./build/bin/uart_loopback /dev/ttyTHS1 115200 "jetson-uart-test"
```

장치 이름은 DT와 릴리스에 따라 달라질 수 있으므로 `/dev/ttyTHS1`을 복사해 고정하지 않는다. `systemctl`, kernel command line, `lsof`로 콘솔이나 다른 서비스가 포트를 사용 중인지 확인한다.

## 2.4 실무 확장

- GNSS: NMEA 텍스트는 줄 단위 parser와 checksum 검증을 추가한다.
- MCU protocol: frame magic, length, sequence, CRC, timeout, 재동기화를 설계한다.
- 로봇 안전: UART command가 끊기면 actuator를 정지시키는 heartbeat/watchdog를 둔다.
- 장거리·노이즈 환경: TTL UART 대신 절연 RS-485 transceiver와 차동 배선을 고려한다.

`read()` 한 번이 프레임 한 개를 반환한다는 보장은 없다. 실제 프로그램은 ring buffer에 누적하고 protocol framing으로 분리한다.

