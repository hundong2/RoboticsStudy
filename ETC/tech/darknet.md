# Hank.ai / CCodeRun Darknet YOLO 정리

## 사이트

- 기존 링크: <https://github.com/hank-ai/darknet>
- 현재 권장 원본 저장소: <https://codeberg.org/CCodeRun/darknet>
- 기존 프로젝트 웹사이트: <https://darknetcv.ai/>
- 현재 문서/FAQ: <https://www.ccoderun.ca/programming/darknet_faq/>
- 관련 도구
  - DarkHelp: <https://codeberg.org/CCodeRun/DarkHelp>
  - DarkMark: <https://codeberg.org/CCodeRun/DarkMark>

## 한 줄 요약

Darknet은 C/C++/CUDA 기반의 오픈소스 객체 탐지 프레임워크이고, YOLO 계열 모델을 학습하거나 이미지/영상/웹캠에서 실시간 추론하는 데 쓰인다. 파일에 적힌 `hank-ai/darknet` GitHub 저장소는 2025년 8월 이후 Codeberg의 `CCodeRun/darknet` 저장소를 자동 미러링하는 위치로 보는 것이 좋다.

## 무엇을 하는 프로젝트인가

Darknet은 크게 세 가지 용도로 사용된다.

1. 이미지 또는 영상에서 객체를 탐지한다.
2. 사용자가 직접 라벨링한 데이터로 YOLO 모델을 학습한다.
3. C/C++/Python 등 애플리케이션에서 객체 탐지 기능을 라이브러리로 통합한다.

YOLO는 "You Only Look Once"의 약자로, 입력 이미지를 한 번의 네트워크 통과로 처리해 객체의 종류와 위치를 동시에 예측하는 실시간 객체 탐지 방식이다. Darknet은 이 YOLO 모델을 실행하고 학습하는 엔진 역할을 한다.

## 저장소의 현재 상태

- 원래 Darknet은 Joseph Redmon의 프로젝트에서 시작했다.
- 이후 AlexeyAB 포크가 YOLOv4와 YOLOv7 흐름을 만들었다.
- 2023년부터 Hank.ai 후원으로 Stéphane Charette가 유지보수한 포크가 현대화되었다.
- 2025년 8월 Darknet v5.0부터는 Codeberg의 `CCodeRun/darknet`이 권장 원본 저장소이고, GitHub의 `hank-ai/darknet`은 미러 역할을 한다.
- 2026년 7월 기준 FAQ에는 v6 "Winston"이 개발 브랜치이며 2026년 출시 예정으로 설명되어 있다.

## 주요 특징

- C++17, CMake, OpenCV 기반으로 빌드한다.
- Linux, Windows, macOS, WSL, Docker, Google Colab 환경을 지원한다.
- NVIDIA GPU는 CUDA/cuDNN으로 가속할 수 있다.
- AMD GPU는 ROCm/HIP 지원이 추가되었지만, Windows ROCm/HIP 지원은 아직 명확하지 않다.
- CPU 전용 빌드에서는 OpenBLAS를 사용해 성능을 높일 수 있다.
- v5 계열부터 ONNX export가 실험적으로 제공된다.
- Darknet v3 이후 C/C++ API와 예제 앱이 정리되어 애플리케이션 통합이 쉬워졌다.

## 버전 흐름

- v0: Joseph Redmon의 원본 Darknet.
- v1: AlexeyAB 포크. YOLOv4, YOLOv7 흐름과 관련이 깊다.
- v2 "Oak": 2023년 Hank.ai 후원 포크. CMake 기반 빌드와 C++ 컴파일러 전환이 진행되었다.
- v3 "Jazz": 2024년 릴리스. 학습과 추론 성능 개선, API 정리, 오래된 명령 제거.
- v4 "Slate": 2025년 초 릴리스. AMD ROCm 지원과 로깅 구조 개선.
- v5 "Moonlit": 2025년 8월 릴리스. Codeberg 이전, OpenBLAS CPU 최적화, PGO, ONNX export, Java 바인딩 작업.
- v5.1: 2025년 12월 릴리스. ONNX export에서 `confs`와 `boxes` 노드 export 개선, mAP 함수 재작성, 추가 성능 최적화.
- v6 "Winston": 2026년 기준 개발 브랜치.

## 설치 방법 개요

### Linux / WSL 권장 흐름

Windows 사용자라도 가능하면 WSL2 + Ubuntu 24.04 LTS를 사용하는 것이 권장된다. GPU를 쓰려면 WSL용 NVIDIA 드라이버가 필요하다.

```bash
sudo apt-get update
sudo apt-get install build-essential git libopencv-dev cmake libprotobuf-dev protobuf-compiler

mkdir -p ~/src
cd ~/src
git clone https://codeberg.org/CCodeRun/darknet.git
cd darknet
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4 package
sudo dpkg -i darknet-*.deb
darknet version
```

CPU 전용 빌드에서 성능을 올리고 싶다면 OpenBLAS 설치를 고려한다.

```bash
sudo apt-get install libopenblas64-0 libopenblas64-0-openmp libopenblas64-openmp-dev
```

### Windows 네이티브 빌드

Windows에서는 PowerShell보다 Visual Studio Developer Command Prompt 사용이 권장된다.

```bat
winget install Git.Git
winget install Kitware.CMake
winget install nsis.nsis
winget install Microsoft.VisualStudio.2022.Community
```

그 다음 Visual Studio Installer에서 `Desktop Development With C++`를 선택한다. 의존성은 vcpkg로 설치한다.

```bat
cd c:\
mkdir c:\src
cd c:\src
git clone https://github.com/microsoft/vcpkg
cd vcpkg
bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
.\vcpkg.exe install opencv[contrib,dnn,freetype,jpeg,openmp,png,webp,world]:x64-windows protobuf:x64-windows
```

Darknet 빌드:

```bat
cd c:\src
git clone https://codeberg.org/CCodeRun/darknet.git
cd darknet
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake ..
msbuild.exe /property:Platform=x64;Configuration=Release /target:Build -maxCpuCount -verbosity:normal -detailedSummary darknet.sln
msbuild.exe /property:Platform=x64;Configuration=Release PACKAGE.vcxproj
```

빌드만 한 상태와 설치한 상태는 다르다. Windows에서는 생성된 NSIS 설치 파일을 실행해 `darknet.exe`, DLL, 헤더, cfg 템플릿을 제대로 설치해야 한다.

### Docker

GPU Docker 빌드는 `nvidia-container-toolkit`이 필요하다. CUDA/cuDNN 기반 이미지에서 Darknet을 클론하고 빌드하는 방식이다.

## 기본 사용 방법

### 버전 확인

```bash
darknet version
darknet help
```

### 사전 학습 가중치로 테스트

처음 설치 확인에는 MSCOCO, People-R-People, LEGO Gears 같은 사전 학습 가중치를 사용하면 빠르다. MSCOCO는 80개 일반 객체 클래스를 포함하고, People-R-People은 `person`, `head` 2개 클래스를 대상으로 한다.

예시:

```bash
wget --no-clobber https://github.com/hank-ai/darknet/releases/download/v2.0/yolov4-tiny.weights
darknet_02_display_annotated_images coco.names yolov4-tiny.cfg yolov4-tiny.weights image1.jpg
darknet_03_display_videos coco.names yolov4-tiny.cfg yolov4-tiny.weights video1.avi
```

### 이미지 추론

Darknet v3 이후 예제 앱을 쓰는 흐름:

```bash
darknet_02_display_annotated_images cars.cfg image1.jpg
```

DarkHelp를 쓰는 흐름:

```bash
DarkHelp cars.names cars.cfg cars_best.weights image1.jpg
```

### JSON 출력

```bash
darknet_06_images_to_json animals image1.jpg
DarkHelp --json animals.names animals.cfg animals_best.weights image1.jpg
```

### 영상 또는 웹캠 처리

```bash
darknet_03_display_videos animals.cfg test.mp4
darknet_08_display_webcam animals
```

### 정확도 평가

```bash
darknet detector map animals.data animals.cfg animals_best.weights
darknet detector map animals.data animals.cfg animals_best.weights -iou_thresh 0.75
```

### ONNX export

v5 계열에서 ONNX export가 실험적으로 제공된다.

```bash
darknet_onnx_export cars.cfg
```

ONNX로 변환하면 ONNX Runtime, TensorRT, OpenVINO 같은 배포 런타임과 연결할 여지가 생긴다. 다만 Darknet의 ONNX export는 프로젝트 문서상 아직 실험적 기능이므로 실제 제품 적용 전 검증이 필요하다.

## 직접 학습하는 방법

권장 흐름은 DarkMark로 프로젝트를 만들고 라벨링과 학습 파일 생성을 관리하는 것이다. 수동으로 구성한다면 다음 파일들이 필요하다.

- `*.names`: 클래스 이름 목록. 한 줄에 하나씩 작성한다.
- `*.data`: 클래스 수, train/valid 목록, names 파일, backup 경로를 정의한다.
- `*.cfg`: 네트워크 구조와 학습 파라미터를 정의한다.
- 이미지 파일과 같은 이름의 `*.txt`: YOLO 형식 라벨 파일.
- `train.txt`, `valid.txt`: 학습/검증 이미지 경로 목록.

예시 `animals.names`:

```txt
dog
cat
bird
horse
```

예시 `animals.data`:

```txt
classes = 4
train = /home/username/nn/animals/animals_train.txt
valid = /home/username/nn/animals/animals_valid.txt
names = /home/username/nn/animals/animals.names
backup = /home/username/nn/animals
```

학습 명령:

```bash
darknet detector -map -dont_show train animals.data animals.cfg
```

## cfg 선택 가이드

초기 프로젝트나 실시간성이 중요한 프로젝트는 `yolov4-tiny.cfg`를 먼저 검토하는 것이 실용적이다. FAQ 기준으로도 2026년 현재 Stéphane Charette는 YOLOv4-tiny를 YOLOv7-tiny나 일부 Python 기반 YOLO 구현보다 여전히 추천한다. 객체가 작거나 서로 붙어 있어 구분이 어렵다면 `yolov4-tiny-3l.cfg`를 고려한다. full `yolov4.cfg`, `yolov7.cfg`는 느리고 무거우므로 특별한 이유가 있을 때만 쓴다.

네트워크 입력 크기는 반드시 정사각형일 필요는 없지만, width와 height는 32로 나누어 떨어져야 한다. 크기를 키우면 작은 객체 탐지에는 유리하지만 느려지고 GPU 메모리를 더 쓴다. 크기를 줄이면 빠르지만 작은 객체를 놓치기 쉽다.

## 최신 기술 동향

### Darknet 내부 최신 흐름

- 저장소 중심이 GitHub에서 Codeberg로 이동했다.
- v5 계열에서 CPU-only 성능을 위한 OpenBLAS, PGO, 추가 성능 최적화가 들어갔다.
- ONNX export가 추가되어 Darknet 모델을 다른 배포 생태계로 옮기는 흐름이 강화되었다.
- AMD ROCm/HIP 지원이 들어왔지만 NVIDIA CUDA/cuDNN 대비 운영 안정성은 환경별 확인이 필요하다.
- v6 개발 브랜치에서는 2026년 릴리스를 목표로 추가 개선이 진행 중이다.

### YOLO 계열 전체 동향

Darknet/YOLO와 별개로 Python 기반 YOLO 생태계도 빠르게 변하고 있다.

- Ultralytics YOLO11은 detection, segmentation, classification, pose estimation, oriented bounding box 같은 다양한 작업을 한 생태계에서 지원한다.
- Ultralytics YOLO26은 2026년 기준 최신 Ultralytics 계열로 소개되며, NMS-free end-to-end inference, edge deployment 최적화, DFL-free regression, small-target-aware label assignment 같은 방향을 강조한다.
- YOLOv12는 attention-centric real-time detector를 표방한다. 기존 YOLO가 CNN 중심 개선에 치우쳤던 흐름에서 attention을 실시간 탐지 속도에 맞게 적용하려는 시도다.
- 최신 객체 탐지 배포에서는 ONNX, TensorRT, OpenVINO, CoreML 등으로 export한 뒤 하드웨어별 런타임에서 최적화하는 방식이 중요하다.
- 로봇/엣지 환경에서는 최고 정확도보다 지연 시간, 전력, 메모리, 프레임 안정성이 더 중요할 때가 많다.

## Darknet을 선택하면 좋은 경우

- C/C++ 기반 애플리케이션에 객체 탐지를 직접 넣고 싶을 때.
- Linux/Windows/WSL에서 빠른 추론과 비교적 낮은 런타임 의존성을 원할 때.
- 기존 Darknet/YOLO `.cfg`, `.weights`, `.names` 자산이 있을 때.
- 연구용보다 실시간 영상 처리, 산업 검사, 로봇 비전처럼 가볍고 빠른 실행이 중요한 경우.

## 다른 YOLO 구현을 선택하는 편이 나은 경우

- Python 중심으로 빠르게 실험하고 싶을 때.
- segmentation, pose, OBB, tracking 등을 하나의 고수준 API로 쓰고 싶을 때.
- 최신 Ultralytics YOLO11/YOLO26 생태계, 학습 로그, 클라우드/배포 도구와의 통합이 더 중요할 때.
- 라이선스와 상용 사용 조건을 별도로 검토할 수 있고, 해당 생태계가 요구하는 의존성을 감당할 수 있을 때.

## 실전 추천 흐름

1. 설치 확인은 사전 학습 MSCOCO 또는 People-R-People weights로 한다.
2. 새 프로젝트라면 DarkMark로 라벨링과 학습 파일을 만든다.
3. 첫 모델은 `yolov4-tiny.cfg` 또는 프로젝트에 맞는 tiny 계열로 시작한다.
4. mAP, precision, recall, false positive/false negative를 확인한다.
5. 작은 객체가 문제면 입력 해상도, tiling, `yolov4-tiny-3l.cfg`, 라벨 품질을 먼저 점검한다.
6. 배포가 목표라면 FPS뿐 아니라 지연 시간, 메모리, 장시간 안정성, 카메라 입력 파이프라인까지 같이 측정한다.
7. 다른 런타임 배포가 필요하면 ONNX export를 테스트하되, v5 계열에서는 실험적 기능임을 고려해 실제 출력값을 반드시 비교한다.

## 실습 프로젝트

바로 실습할 수 있도록 이 문서 옆에 `darknet_practice` 폴더를 만들었다.

```txt
D:\workspace\RoboticsStudy\ETC\tech\darknet_practice
```

구성은 다음과 같다.

- `README.md`: 실습 순서 전체 설명.
- `scripts/setup_darknet_wsl.sh`: WSL2 Ubuntu에서 Darknet을 클론하고 빌드하는 스크립트.
- `scripts/run_coco_image.ps1`: 사전 학습 `yolov4-tiny.weights`로 이미지 추론을 시도하는 PowerShell 스크립트.
- `scripts/create_dataset_lists.ps1`: `data/images/train`, `data/images/valid`의 이미지 목록을 `project/train.txt`, `project/valid.txt`로 생성하는 스크립트.
- `scripts/train_robot_parts_wsl.sh`: 로봇 부품 예제 데이터셋 학습을 시작하는 WSL용 스크립트.
- `project/robot_parts.names`: 예제 클래스 이름. 기본값은 `bolt`, `nut`, `washer`.
- `project/robot_parts.data`: Darknet 학습용 data 파일.
- `data/images/*`, `data/labels/*`: 직접 이미지와 YOLO 라벨을 넣는 위치.
- `weights`: 다운로드한 weights와 학습 결과 저장 위치.
- `outputs`: 추론 결과 저장 위치.

### 실습 1: 설치

WSL2 Ubuntu에서 실행한다.

```bash
cd /mnt/d/workspace/RoboticsStudy/ETC/tech/darknet_practice
bash scripts/setup_darknet_wsl.sh
sudo dpkg -i ~/src/darknet/build/darknet-*.deb
darknet version
```

### 실습 2: 샘플 이미지 추론

Windows PowerShell에서 실행한다.

```powershell
cd D:\workspace\RoboticsStudy\ETC\tech\darknet_practice
.\scripts\run_coco_image.ps1 -ImagePath D:\workspace\RoboticsStudy\CV.jpeg
```

Darknet이 아직 PATH에 없으면 스크립트가 실행해야 할 명령을 출력한다. 설치가 되어 있으면 `yolov4-tiny.weights`를 받아서 COCO 객체 탐지를 실행한다.

### 실습 3: 내 데이터셋 학습

이미지와 라벨을 아래 위치에 넣는다.

```txt
data/images/train/
data/images/valid/
data/labels/train/
data/labels/valid/
```

그 다음 목록 파일을 만든다.

```powershell
.\scripts\create_dataset_lists.ps1
```

Darknet 저장소의 `cfg/yolov4-tiny.cfg`를 `project/robot_parts.cfg`로 복사하고, 클래스 수에 맞게 마지막 YOLO layer들의 `classes`와 `filters`를 수정한다. 기본 클래스가 3개이므로 `classes=3`, `filters=24`가 된다.

학습은 WSL에서 실행한다.

```bash
cd /mnt/d/workspace/RoboticsStudy/ETC/tech/darknet_practice
bash scripts/train_robot_parts_wsl.sh
```

## 참고 자료

- Hank.ai GitHub mirror: <https://github.com/hank-ai/darknet>
- Current Codeberg repository: <https://codeberg.org/CCodeRun/darknet>
- Darknet README: <https://github.com/hank-ai/darknet/blob/master/README.md>
- Darknet FAQ: <https://www.ccoderun.ca/programming/darknet_faq/>
- NVIDIA GPU README: <https://github.com/hank-ai/darknet/blob/master/README_GPU_NVIDIA_CUDA.md>
- AMD ROCm README: <https://github.com/hank-ai/darknet/blob/master/README_GPU_AMD_ROCM.md>
- Ultralytics YOLO11: <https://docs.ultralytics.com/models/yolo11>
- Ultralytics YOLO26: <https://docs.ultralytics.com/models/yolo26>
- YOLOv12 paper: <https://arxiv.org/abs/2502.12524>
