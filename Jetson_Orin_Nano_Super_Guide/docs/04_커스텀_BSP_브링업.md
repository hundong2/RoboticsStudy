# 4. 커스텀 BSP와 캐리어보드 브링업

## 4.1 BSP의 범위

Jetson Linux BSP는 부트 펌웨어, UEFI, Linux 커널, Ubuntu rootfs, NVIDIA 드라이버, device tree, 플래시 도구와 저수준 플랫폼 설정을 포함한다. P3768 기준 캐리어보드에서 개발만 한다면 수정 없이 사용할 수 있지만, 생산용 캐리어보드로 옮기면 다음 항목을 보드에 맞춰야 한다.

- kernel device tree
- MB1 BCT(핀mux, GPIO, pad voltage, 전원 관련 부트 설정)
- MB2 설정
- ODM data와 UPHY lane 구성
- EEPROM/board ID 정책
- flash configuration과 partition layout
- USB, PCIe, NVMe, Ethernet, display, camera, fan, 전원 시퀀스

## 4.2 기준 자료

1. Jetson Orin NX Series and Orin Nano Series Design Guide
2. Jetson Orin Nano Developer Kit Carrier Board Specification
3. Carrier Board Reference Design Files(회로도, PCB, BOM)
4. Jetson Orin NX/Nano Pinmux Configuration Template
5. Jetson Linux r39.2.1 Developer Guide의 Module Adaptation and Bring-Up
6. Jetson Orin Technical Reference Manual(TRM)
7. 모듈 Data Sheet 및 Thermal Design Guide

하드웨어 설계 능력과 BSP 지원 범위는 다를 수 있다. Design Guide에 전기적으로 가능한 인터페이스가 있어도 현재 Jetson Linux 릴리스가 해당 조합을 지원하는지 Software Feature Overview와 Release Notes에서 확인한다.

## 4.3 보드 명명과 형상관리

커스텀 보드 이름은 소문자 영숫자, 하이픈, 밑줄을 사용하고 공백을 피한다. 다음을 한 세트로 버전 관리한다.

- 보드 이름과 revision
- SOM SKU/P-number
- carrier board ID와 EEPROM 내용
- pinmux spreadsheet 원본과 생성된 `.dtsi`
- device tree source/overlay
- MB1/MB2 BCT 변경
- flash `.conf` 파일
- partition XML
- kernel config 및 out-of-tree 모듈
- rootfs customization 스크립트
- 플래시 로그와 검증 결과

NVIDIA 원본 파일을 직접 덮어쓰기보다 커스텀 파일을 별도 디렉터리에 두고, 원본 릴리스 태그와 diff를 유지한다.

## 4.4 권장 브링업 순서

### 단계 1: 전원 전용 검증

- SOM 미장착 상태에서 전원 레일, 시퀀스, 리플, inrush, 역극성 보호를 검증한다.
- 각 레일의 최대·최소와 ramp timing을 Design Guide와 대조한다.
- DC 입력, power-good, shutdown request, reset 신호를 오실로스코프로 확인한다.

### 단계 2: 최소 부팅

- 디버그 UART를 먼저 확보한다.
- QSPI/UEFI, RAM, 기본 rootfs가 부팅하는지 확인한다.
- 부트 로그를 원본 P3768 로그와 비교한다.

### 단계 3: 저속 인터페이스

- EEPROM, I2C, SPI, UART, GPIO, fan tach/PWM을 순차 검증한다.
- 각 기능마다 pinmux와 device tree node가 동시에 맞는지 확인한다.

### 단계 4: 고속 인터페이스

- USB, PCIe/NVMe, Ethernet, CSI, DisplayPort 순으로 lane mapping과 signal integrity를 확인한다.
- UPHY lane은 한 기능의 변경이 다른 PCIe/USB 포트에 영향을 줄 수 있으므로 전체 lane map을 표로 관리한다.

### 단계 5: 열·전력·복구

- 모든 전력 모드에서 스트레스 테스트한다.
- 팬 고장, 센서 단절, 저전압, 과열, 강제 복구, 전원 재인가를 시험한다.

## 4.5 Pinmux와 GPIO

공식 Pinmux spreadsheet에서 보드 설정을 만들고 생성된 pinmux/gpio dtsi를 BSP의 T234 BCT 경로에 반영한다. JetPack 6 이후에는 Jetson-IO 또는 pinmux dtsi 방식이 가능하지만, 생산 보드의 부트 초기 상태와 pad voltage는 MB1 적용 여부를 분명히 해야 한다.

GPIO 식별 절차:

1. module pin name을 Design Guide/Pin Function Names Guide에서 확인한다.
2. SoC pad와 GPIO port를 TRM에서 확인한다.
3. pinmux에서 GPIO 기능, 방향, pull, tristate를 설정한다.
4. 생성된 dtsi와 최종 DTB에 값이 들어갔는지 검사한다.
5. 런타임에서는 character-device GPIO API와 `gpioinfo`를 사용한다.

`/sys/class/gpio` 숫자를 다른 JetPack 버전에서 그대로 복사하지 않는다.

## 4.6 Device Tree

장치 노드에는 최소한 compatible, reg/address, interrupt, clocks, resets, regulators, pinctrl, status를 정확히 기술한다. overlay를 사용할 경우 base DTB와 overlay의 symbol/fragment 호환성을 확인한다.

검증 예:

```bash
sudo dtc -I fs -O dts /proc/device-tree > running-device-tree.dts
grep -R "status = \"okay\"" running-device-tree.dts
dmesg -T | grep -Ei 'error|fail|timeout|i2c|spi|pcie|camera'
```

## 4.7 커널과 드라이버

- r39.2의 정확한 kernel source/tag와 toolchain을 사용한다.
- 기본 커널 ABI와 NVIDIA out-of-tree 모듈의 버전을 맞춘다.
- 카메라 센서 드라이버는 loadable kernel module 패키지로 관리하는 것을 우선 고려한다.
- source, config, DTB, modules, initrd를 동일 빌드 ID로 묶는다.
- 설치 전에 복구 가능한 기존 커널/DTB와 UART 콘솔을 확보한다.

## 4.8 Rootfs 커스터마이징과 재현성

rootfs에 손으로 패키지를 설치한 뒤 이미지를 복제하는 방식보다, 패키지 목록·APT pin·사용자 생성·systemd 서비스·컨테이너 이미지를 스크립트로 재현한다. `nv_customize_rootfs.sh`, Debian 패키지, cloud-init 또는 자체 provisioning을 사용하되 비밀번호와 키를 이미지에 하드코딩하지 않는다.

## 4.9 양산 전 체크리스트

- initrd flash 기반의 양산 이미지와 massflash 절차 검증
- A/B rootfs 및 OTA 전략
- Secure Boot, 키 보관, 디스크 암호화 여부 결정
- 고유 장치 ID, 인증서, SSH host key 생성 정책
- watchdog, crash log, health telemetry
- factory test: RAM, storage, USB, Ethernet, CSI, GPIO, thermal, fan
- 라이선스 고지와 소스 제공 의무 검토
- 전원·EMC·열·기구·케이블 스트레인 릴리프 검증

퓨즈 기반 Secure Boot는 되돌릴 수 없는 단계가 포함될 수 있다. 개발키트 한 대로 즉시 적용하지 말고 키 복구·RMA·양산 서명 체계를 먼저 확정한다.

