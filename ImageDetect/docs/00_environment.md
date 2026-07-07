# 00. 환경 준비

Windows + PowerShell 기준입니다. 저장소 루트는 `D:\workspace\RoboticsStudy`로 가정합니다.

## Python 실습 환경

MediaPipe와 RT-DETR Python 예제를 실행할 때 사용합니다.

Python은 `3.11` 또는 `3.12`를 권장합니다. MediaPipe/OpenCV는 최신 Python보다 안정 지원 버전에서 설치가 더 매끄럽습니다.

```powershell
cd D:\workspace\RoboticsStudy\ImageDetect
py -3.11 -m venv .venv
.\.venv\Scripts\activate
python -m pip install --upgrade pip
pip install -r requirements.txt
```

RT-DETR Hugging Face 예제까지 실행하려면 추가 패키지를 설치합니다.

```powershell
pip install -r requirements-rtdetr.txt
```

## C# 스크립트 환경

OpenCVSharp 기반 `.csx` 예제를 실행할 때 사용합니다.

```powershell
dotnet --version
dotnet tool install -g dotnet-script
dotnet script .\examples\csharp\camera_preview.csx
```

`dotnet script` 명령을 찾지 못하면 새 PowerShell 창을 열거나, 사용자 전역 도구 경로가 `PATH`에 들어갔는지 확인합니다.

## 모델과 이미지 다운로드

샘플 이미지는 이미 `assets/images`에 들어 있습니다. 다시 받을 때는 다음 명령을 사용합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\download_sample_images.ps1
```

경량 실습 모델은 이미 `assets/models/efficientdet_lite0_int8.tflite`에 들어 있습니다. YOLOX ONNX는 필요할 때 내려받습니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\download_models.ps1 -Model yolox_tiny
powershell -ExecutionPolicy Bypass -File .\scripts\download_models.ps1 -Model yolox_s
```

## 카메라 문제 해결

- 기본 카메라는 보통 `--camera 0`입니다.
- 외장 카메라가 여러 개면 `--camera 1`, `--camera 2`를 순서대로 시도합니다.
- Windows 개인정보 설정에서 데스크톱 앱의 카메라 접근이 허용되어야 합니다.
- WSL 내부가 아니라 Windows PowerShell에서 실행해야 웹캠 접근이 안정적입니다.
- 회사 보안 프로그램이나 화상회의 앱이 카메라를 점유 중이면 예제가 프레임을 읽지 못할 수 있습니다.
