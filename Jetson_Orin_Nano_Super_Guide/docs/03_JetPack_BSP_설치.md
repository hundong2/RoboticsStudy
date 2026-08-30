# 3. JetPack과 BSP 설치

## 3.1 기준 버전

2026-08-29 기준 공식 최신 개발키트 경로는 JetPack 7.2.1, Jetson Linux 39.2.1이다. JetPack에는 BSP와 CUDA, cuDNN, TensorRT, VPI, 멀티미디어 API, 컨테이너 런타임 등 가속 소프트웨어가 포함된다.

현재 상태 확인:

```bash
cat /etc/nv_tegra_release
dpkg-query -W nvidia-jetpack 2>/dev/null || true
uname -a
```

JetPack 7.2 시스템은 R39 계열을 보고해야 한다. 프로젝트가 JetPack 6.x에 고정되어 있다면 r36.5 문서를 별도 기준선으로 유지하고 r39 명령을 섞지 않는다.

## 3.2 저장장치 선택

- microSD: 64GB UHS-1 이상 권장. 입문과 복구가 쉽지만 지속적인 컨테이너·데이터 기록에는 한계가 있다.
- NVMe: Vision VLA 개발의 기본 권장. 모델, rosbag, Docker overlay, 빌드 캐시 때문에 256GB 이상, 실무는 512GB 이상이 편하다.
- USB 저장장치: 일부 플래시/부팅 경로에서 지원되지만 로봇의 진동·케이블 신뢰성과 성능을 고려한다.

백업 없이 저장장치 플래시를 실행하지 않는다. `lsblk -o NAME,SIZE,MODEL,SERIAL,MOUNTPOINTS`로 대상 장치를 두 번 확인한다.

## 3.3 JetPack 7.2.1 권장: Jetson ISO

JetPack 7.2부터 개발키트용 SD Card Image 방식 대신 Jetson ISO 설치 USB를 사용한다. ISO 파일을 microSD에 직접 기록하면 안 된다.

1. PC에서 공식 JetPack 7.2.1 ISO를 받는다.
2. 16GB 이상 USB 메모리에 ISO를 기록한다.
3. Jetson의 대상 microSD 또는 NVMe를 장착한다.
4. UEFI/QSPI 펌웨어가 36.x 세대 이상인지 확인한다.
5. USB 설치 미디어로 부팅해 대상 저장장치에 Jetson Linux를 설치한다.
6. 초기 설정과 EULA를 완료한다.
7. 설치 후 `sudo apt update && sudo apt dist-upgrade`를 수행한다.
8. 필요한 JetPack 구성 요소를 설치한다.

```bash
sudo apt update
sudo apt install nvidia-jetpack
sudo reboot
```

## 3.4 구형 출고 펌웨어의 QSPI 업데이트

JetPack 7.2 ISO는 JetPack 6.x 세대 UEFI/QSPI 펌웨어가 필요하다. UEFI 메뉴에서 펌웨어 버전을 확인한다.

- 모니터: DisplayPort와 키보드를 연결하고 NVIDIA 로고 직후 Esc를 반복 입력한다.
- 헤드리스: J14 디버그 UART(3 RX, 4 TX, 7 GND)를 연결하고 부팅 중 Esc를 입력한다.
- 36.0보다 오래된 펌웨어라면 공식 `JetPack 6.x Update Path`를 먼저 수행한다.

공식 업데이트 경로의 핵심은 구형 부트 펌웨어가 읽을 수 있는 JetPack 5.1.3 microSD 이미지로 먼저 부팅하고, 패키지 업데이트를 통해 QSPI를 JetPack 6.x 세대로 올린 다음, 최종 JetPack 설치 미디어로 전환하는 것이다. 중간 재부팅과 QSPI 갱신이 끝나기 전에 전원을 차단하지 않는다.

## 3.5 SDK Manager 경로

SDK Manager는 Ubuntu x86_64 호스트가 필요하다.

1. 호스트에 SDK Manager를 설치한다.
2. Target Hardware에서 Jetson Orin Nano를 선택한다.
3. Jetson을 Force Recovery Mode로 전환한다.
4. 저장장치로 NVMe 또는 SD Card를 선택한다.
5. OEM Configuration은 일반적으로 Runtime을 선택한다.
6. 플래시 완료 후 J14 recovery 점퍼를 제거한다.
7. 초기 부팅을 완료한 뒤 필요하면 SDK Manager로 추가 JetPack 구성요소를 설치한다.

## 3.6 명령행 BSP/플래시 경로

명령행 경로는 자동화, 커스텀 rootfs, 양산 플래시, 파티션 변경에 사용한다.

1. 해당 Jetson Linux 릴리스의 Driver Package와 Sample Root Filesystem을 받는다.
2. Ubuntu x86_64 호스트에서 `Linux_for_Tegra`를 준비한다.
3. sample rootfs를 `Linux_for_Tegra/rootfs`에 푼다.
4. 공식 스크립트로 NVIDIA 바이너리를 적용한다.
5. 대상 보드를 RCM에 넣는다.
6. r39.2 문서의 `l4t_initrd_flash.sh`를 사용한다.

Orin NX/Nano의 공식 플래시 방식은 initrd flash이다. `flash.sh`는 개발 중 일부 파티션 작업에만 사용하고, OTA를 고려하는 제품 이미지는 initrd 레이아웃을 유지한다.

일반 NVMe 플래시 형태:

```bash
cd Linux_for_Tegra
sudo ./tools/kernel_flash/l4t_initrd_flash.sh \
  --external-device nvme0n1p1 \
  -p "-c ./bootloader/generic/cfg/flash_t234_qspi.xml" \
  -c ./tools/kernel_flash/flash_l4t_t234_nvme.xml \
  --showlogs --network usb0 --erase-all \
  jetson-orin-nano-devkit external
```

Super 구성을 명시적으로 사용할 때는 현재 릴리스에 존재하는 `jetson-orin-nano-devkit-super.conf`와 Quick Start의 명령을 확인한다. 보드 설정 이름을 임의로 추측하지 않는다.

## 3.7 설치 검증

```bash
cat /etc/nv_tegra_release
uname -r
lsblk -o NAME,SIZE,MODEL,MOUNTPOINTS
sudo /usr/sbin/nvpmodel -q
sudo tegrastats
nvcc --version || true
python3 - <<'PY'
try:
    import tensorrt as trt
    print('TensorRT', trt.__version__)
except Exception as exc:
    print('TensorRT 확인 실패:', exc)
PY
```

검증 결과와 설치 날짜, ISO/Driver Package 체크섬, board SKU, 저장장치 모델을 프로젝트 운영 문서에 남긴다.

