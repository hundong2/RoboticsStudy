# 5. CAN과 SocketCAN

## 5.1 핵심 개념

CAN controller는 frame을 만들지만 물리 버스의 차동 신호는 transceiver가 만든다.

```text
Jetson J17 CAN_TX ──> transceiver TXD     CAN_H ───── CAN_H
Jetson J17 CAN_RX <── transceiver RXD     CAN_L ───── CAN_L
Jetson J17 3.3V/GND ─ transceiver         양 끝에 120 ohm termination
```

J17 핀 1/2는 공식 r39.2 계열 개발자 가이드의 표기와 Carrier Spec 표가 서로 뒤바뀌어 보일 수 있다. **이 저장소는 Carrier Board Specification SP-11324-001 v1.3의 J17 표(`1=CAN_TX`, `2=CAN_RX`)를 기준으로 하되, 실제 PCB silkscreen·회로도·측정으로 교차 확인한 뒤 연결한다.**

트랜시버는 controller 쪽 3.3V 로직과 원하는 Classical CAN/CAN FD 속도를 지원해야 한다. 버스의 양 끝만 120 ohm으로 종단하며 전원을 끈 상태에서 CAN_H와 CAN_L 사이 약 60 ohm인지 확인한다.

## 5.2 인터페이스 설정

```bash
sudo modprobe can can_raw mttcan
ip -details link show type can
sudo ip link set can0 down 2>/dev/null || true
sudo ip link set can0 type can bitrate 500000 restart-ms 100
sudo ip link set can0 up
ip -details -statistics link show can0
```

CAN FD는 transceiver와 전체 노드가 지원할 때만 설정한다.

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 dbitrate 2000000 fd on
sudo ip link set can0 up
```

## 5.3 C++ 송수신

두 노드 또는 적절한 loopback 환경에서 실행한다.

```bash
# 터미널 A
./build/bin/can_socketcan can0 recv

# 터미널 B
./build/bin/can_socketcan can0 send 0x123 0x11 0x22 0x33 0x44

# CAN FD payload
./build/bin/can_socketcan can0 sendfd 0x321 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08
```

수신 예제는 5초 timeout을 둔다. ID는 기본 11비트이며 `0x80000000` flag를 직접 섞지 않는다. production protocol은 ID allocation, rolling counter, CRC, timeout, bus-off recovery, actuator safe state를 정의한다.

## 5.4 문제 판정

- `bus-off`: bitrate 불일치, ACK할 두 번째 노드 부재, 배선·종단 문제를 먼저 본다.
- error counter 증가: CAN_H/L 뒤바뀜, stub 길이, ground offset, transceiver standby 핀을 확인한다.
- controller는 보이는데 frame 없음: J17 실장, pinmux, DT status, transceiver TXD/RXD 방향을 확인한다.
- 로봇 제어에서는 통신 회복만으로 자동 재가동하지 말고 상위 safety state machine이 재승인하게 한다.
