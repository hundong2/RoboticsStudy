# 초보자 시작 가이드

이 실습의 목표는 실제 카메라 없이도 다음 흐름을 눈으로 확인하는 것입니다.

```text
예제 탐지 데이터 전송 -> 서버가 수신 -> 브라우저에 장치와 탐지 결과 표시
```

처음에는 Jetson이 없어도 됩니다. Windows 개발 PC만으로 서버 부분을 실행할 수 있습니다.

## 1. 준비물

필수:

- Git
- [.NET 10 SDK](https://dotnet.microsoft.com/download/dotnet/10.0)
- PowerShell 7 또는 Windows PowerShell

C++ 장치 프로그램까지 빌드하려면 다음 중 하나가 추가로 필요합니다.

- Windows: Visual Studio 2022 Build Tools의 **Desktop development with C++** workload
- Ubuntu/Jetson: `build-essential`, CMake 3.22 이상, Ninja

Docker와 MAUI는 첫 실습에는 필요하지 않습니다.

## 2. 프로젝트 폴더로 이동

저장소를 받은 뒤 PowerShell에서 실행합니다.

```powershell
cd RobotVisionPlatform
dotnet --version
```

`10.`으로 시작하는 버전이 보이면 준비가 된 것입니다.

## 3. 서버 빌드와 테스트

```powershell
./scripts/build.ps1
./scripts/test.ps1
```

`빌드했습니다`, `오류 0개`, `Server core tests passed`가 표시되면 성공입니다. C++ compiler가 없다는 경고는 서버만 실습할 때는 무시해도 됩니다.

## 4. 서버 실행

```powershell
dotnet run --project server/src/RobotVision.Server.Api
```

터미널이 실행 상태로 유지되는 것이 정상입니다. 브라우저에서 아래 주소를 엽니다.

- 관제 화면: <http://localhost:5080>
- 상태 확인: <http://localhost:5080/health>

처음에는 장치가 없다는 메시지가 표시됩니다.

## 5. 예제 장치 데이터 전송

서버 터미널은 그대로 두고 새 PowerShell 창을 열어 같은 프로젝트 폴더로 이동한 뒤 실행합니다.

```powershell
./scripts/send-demo-event.ps1
```

다시 관제 화면을 보면 `beginner-demo-001` 장치와 `person` 탐지 결과가 나타납니다. 화면은 2초마다 새로고침됩니다.

서버를 종료하려면 서버가 실행 중인 터미널에서 `Ctrl+C`를 누릅니다. 서버를 재시작하면 현재 장치 목록이 사라지는 것이 정상입니다. 아직 데이터베이스를 연결하지 않은 MVP이기 때문입니다.

## 6. C++ 장치 프로그램 실행

Windows에서는 **Developer PowerShell for Visual Studio**를 열어 다음 명령을 사용합니다.

```powershell
cmake -S device -B device/build
cmake --build device/build --config Release
ctest --test-dir device/build -C Release --output-on-failure
./device/build/Release/robot_vision_device.exe --device-id my-first-device
```

Ubuntu 또는 Jetson에서는 다음과 같습니다.

```bash
cmake -S device -B device/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build device/build
ctest --test-dir device/build --output-on-failure
./device/build/robot_vision_device --device-id my-first-device
```

현재 C++ 프로그램은 실제 카메라 대신 일정한 속도로 가짜 프레임을 만듭니다. 세 프레임마다 `demo-object` 하나를 탐지한 것처럼 출력합니다. 실제 카메라와 TensorRT 연결은 README의 Phase 1 작업입니다.

## 다음 학습 순서

1. [비전 학습 기초](../tips/fundamentals/vision-training-basics.md)
2. [데이터셋 만들기](../tips/data/dataset-engineering.md)
3. [Fine-tuning 실습 순서](../tips/training/fine-tuning-playbook.md)
4. [전체 아키텍처](architecture.md)
5. [Jetson 배포](jetson-deployment.md)

문제가 생기면 [문제 해결 문서](troubleshooting.md)를 먼저 확인합니다.

