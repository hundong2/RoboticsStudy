# 문제 해결

## `dotnet` 명령을 찾을 수 없음

.NET 10 **SDK**를 설치하고 새 터미널을 엽니다. Runtime만 설치하면 빌드할 수 없습니다. `dotnet --info`로 SDK 목록을 확인합니다.

## C++ compiler를 찾을 수 없음

Windows에서는 Visual Studio Build Tools와 **Desktop development with C++** workload를 설치한 뒤 일반 PowerShell이 아니라 **Developer PowerShell for Visual Studio**에서 실행합니다.

Ubuntu/Jetson:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build
```

## CMake가 `nmake`를 찾지 못함

일반 PowerShell에서 MSVC 환경 변수가 설정되지 않았을 때 발생합니다. Developer PowerShell을 사용하거나 Ninja를 설치한 뒤 `-G Ninja`를 지정합니다.

## 5080 포트를 이미 사용 중임

기존 서버를 `Ctrl+C`로 종료합니다. 다른 포트를 사용하려면 다음과 같이 실행합니다.

```powershell
dotnet run --project server/src/RobotVision.Server.Api --urls http://localhost:5090
```

이 경우 데모 스크립트에도 포트를 전달합니다.

```powershell
./scripts/send-demo-event.ps1 -ServerUrl http://localhost:5090
```

## 관제 화면에 장치가 표시되지 않음

1. <http://localhost:5080/health>가 열리는지 확인합니다.
2. 데모 스크립트 결과가 `202`인지 확인합니다.
3. 관제 화면을 새로고침하고 2초 기다립니다.
4. 서버 재시작 후라면 데모 이벤트를 다시 보냅니다.

## MAUI 프로젝트가 빌드되지 않음

MAUI는 첫 실습과 서버 실행에 필요하지 않습니다. 필요한 경우 Visual Studio의 MAUI workload 또는 `dotnet workload install maui`를 설치합니다. 기본 solution은 MAUI를 제외하므로 일반 서버 CI에는 영향을 주지 않습니다.

## Jetson에서 Super Mode가 보이지 않음

구형 출하 펌웨어가 JetPack 6.x와 호환되지 않을 수 있습니다. 임의 패키지 설치보다 [NVIDIA 공식 시작 가이드](https://developer.nvidia.com/embedded/learn/get-started-jetson-orin-nano-devkit)의 펌웨어 확인 절차를 먼저 따릅니다.

