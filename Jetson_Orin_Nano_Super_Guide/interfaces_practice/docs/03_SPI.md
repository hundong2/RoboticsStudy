# 3. SPI 기초와 전이중 루프백

## 3.1 핵심 개념

SPI는 controller가 SCK와 chip-select를 만들고 MOSI/MISO로 동시에 데이터를 교환한다. 주소가 없으므로 각 peripheral에 CS가 필요하다. CPOL/CPHA 조합은 mode 0~3으로 표현한다.

```text
Jetson MOSI ─────> peripheral SDI
Jetson MISO <───── peripheral SDO
Jetson SCK  ─────> peripheral SCK
Jetson CS*  ─────> peripheral CS*
Jetson GND  ────── peripheral GND
```

## 3.2 J12 SPI #0 루프백

전원을 끈 뒤 J12 핀 19(MOSI)와 21(MISO)을 연결한다. 핀 23은 SCK, 핀 24는 CS0*이지만 단순 루프백에는 외부 장치가 없다. Jetson-IO에서 SPI 기능을 활성화하고 재부팅한다.

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
ls -l /dev/spidev*
./build/bin/spi_loopback /dev/spidev0.0 500000 32
```

장치 번호는 예시다. 실제 `/dev/spidevB.C`를 사용한다. 예제는 mode 0, 8 bits/word로 전송하며 수신이 송신 pattern과 같은지 검사한다.

## 3.3 실무 확인

- 모두 `0x00/0xFF`: MISO가 뜨거나 pull 상태만 읽히는지, pinmux와 연결을 확인한다.
- 한 비트씩 밀림: CPOL/CPHA, 최대 clock, setup/hold 시간을 확인한다.
- 속도가 높을 때만 실패: 케이블·GND 귀환·레벨 시프터·drive strength를 검토한다.
- CS timing이 특별한 ADC/센서는 여러 `spi_ioc_transfer`와 `cs_change`를 데이터시트에 맞춰 구성한다.
- 커널 드라이버가 있는 장치는 userspace spidev보다 정식 드라이버와 IIO·input·network subsystem 사용을 우선한다.

