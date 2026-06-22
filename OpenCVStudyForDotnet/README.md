# OpenCVStudyForDotnet

이 프로젝트는 VS Code에서 .NET 환경으로 `ipynb` 노트를 실행하는 예제를 담고 있습니다.

## 준비 사항

- .NET SDK 10 설치
- VS Code 설치
- 확장 설치: `C# Dev Kit`, `Jupyter`, `Polyglot Notebooks`

Ubuntu 22.04 / WSL2에서는 아래 순서로 설치할 수 있습니다.

```bash
sudo apt update
sudo apt install -y software-properties-common
sudo add-apt-repository -y ppa:dotnet/backports
sudo apt update
sudo apt install -y dotnet-sdk-10.0
```

설치 확인:

```bash
dotnet --version
dotnet --list-sdks
dotnet interactive --version
```

## VS Code에서 ipynb 실행하기

1. `OpenCVStudyForDotnet/StartCV/01.CheckVersion.ipynb` 파일을 엽니다.
2. 우측 상단의 커널 선택 메뉴를 클릭합니다.
3. `Polyglot Notebooks` 또는 `C#` 커널을 선택합니다.
4. 첫 셀의 언어가 `polyglot-notebook` 또는 `csharp`인지 확인합니다.
5. 셀을 실행합니다.

예시:

```csharp
#!csharp
System.Console.WriteLine("Hello, C#");
```

## 실행이 안 될 때 확인할 것

- 노트북이 Python 커널로 열려 있지 않은지 확인합니다.
- VS Code를 확장 설치 후 다시 시작합니다.
- `dotnet --version` 결과가 10.x인지 확인합니다.
- `dotnet interactive --version`이 정상 동작하는지 확인합니다.

## 참고

현재 샘플 노트북은 `polyglot-notebook` 셀로 실행되는 상태입니다.