# Jetson Orin Nano Super 준비와 배포

대상은 **NVIDIA Jetson Orin Nano Super Developer Kit**입니다. Developer Kit은 개발/검증용이며 양산 제품은 production module과 carrier/thermal/power 설계를 별도로 검토해야 합니다.

## 1. OS와 펌웨어

1. [공식 시작 가이드](https://developer.nvidia.com/embedded/learn/get-started-jetson-orin-nano-devkit)에서 JetPack 6.x 호환 펌웨어 여부를 먼저 확인합니다. 구형 출하 펌웨어는 6.x SD 이미지와 바로 호환되지 않을 수 있습니다.
2. SDK Manager 또는 공식 SD 이미지를 사용합니다. NVMe를 운영 저장소로 권장합니다.
3. `cat /etc/nv_tegra_release`, `dpkg-query -W nvidia-jetpack`, `nvpmodel -q` 결과를 릴리스 기록에 첨부합니다.
4. Super Mode는 충분한 전원과 냉각을 전제로 설정하고 30분 이상 thermal soak test를 수행합니다.

JetPack, CUDA, cuDNN, TensorRT, DeepStream은 독립적으로 최신 버전을 섞지 않습니다. [DeepStream release notes](https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_Release_notes.html)의 platform compatibility를 기준으로 한 세트를 고정합니다.

## 2. 빌드

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build libboost-system-dev
cmake -S device -B device/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build device/build
ctest --test-dir device/build --output-on-failure
cmake --install device/build --prefix device/stage
```

실제 adapter를 추가할 때 JetPack이 제공하는 GStreamer, CUDA, TensorRT 개발 패키지를 사용합니다. 컨테이너를 쓴다면 Jetson용 L4T/DeepStream base image와 NVIDIA Container Runtime의 호환성을 고정합니다.

## 3. 설치

```bash
sudo install -d -o robotvision -g robotvision /opt/robot-vision/bin /etc/robot-vision
sudo install -m 0755 device/stage/bin/robot_vision_device /opt/robot-vision/bin/
sudo install -m 0640 device/config/device.example.toml /etc/robot-vision/device.toml
sudo install -m 0644 deploy/systemd/robot-vision-device.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now robot-vision-device
```

운영에서는 `DynamicUser`보다 카메라/video 그룹과 인증서 권한을 가진 고정 전용 사용자를 생성합니다. unit의 `ExecStart`는 현재 CLI와 일치하며 TOML parser가 연결되기 전까지 `--device-id`만 사용합니다.

## 4. 확인

```bash
systemctl status robot-vision-device
journalctl -u robot-vision-device -f
tegrastats
```

카메라 분리, 서버 차단, Wi-Fi 전환, 재부팅, 온도 상승 상태를 검증합니다. OTA 전에는 이전 binary/model symlink를 유지해 한 명령으로 rollback할 수 있어야 합니다.

