# C++17+와 AI를 같이 배우는 오픈소스 가이드

이 폴더는 C++17 이상 문법과 AI/LLM 기초를 동시에 익히기 위한 학습 자료입니다.

추천 오픈소스는 `ggml-org/llama.cpp`입니다.

- 저장소: https://github.com/ggml-org/llama.cpp
- 빌드 문서: https://github.com/ggml-org/llama.cpp/blob/master/docs/build.md
- 라이선스: MIT License
- 핵심 이유: 로컬 LLM 추론, GGUF 모델 로딩, 양자화, 샘플링, CPU/GPU 백엔드, CMake 기반 C/C++ 프로젝트 구조를 한 번에 공부할 수 있습니다.

## 폴더 구성

```text
Opensource/
  README.md
  docs/
    01_open_source_choice.md
    02_llama_cpp_learning_guide.md
    03_code_walkthrough.md
  code/
    CMakeLists.txt
    .gitignore
    data/
      knowledge_base.txt
    src/
      tiny_rag.cpp
      sampling_demo.cpp
```

## 빠른 실습

현재 예제 코드는 `llama.cpp`를 다운로드하지 않아도 실행되는 C++17 학습용 미니 프로젝트입니다.

```powershell
cd D:\workspace\RoboticsStudy\Opensource\code
cmake -S . -B build
cmake --build build --config Release
```

현재 이 PC의 일반 PowerShell에서는 CMake가 `NMake Makefiles`를 기본값으로 잡지만 `nmake`를 찾지 못했고, `Visual Studio 17 2022` generator도 설치 인스턴스를 찾지 못했습니다. 빌드가 실패하면 아래 중 하나를 먼저 준비하세요.

- Visual Studio 2022 설치 후 "Desktop development with C++" workload 선택
- "x64 Native Tools Command Prompt for VS" 또는 "Developer PowerShell for VS"에서 다시 실행
- MinGW-w64 또는 LLVM/clang 설치 후 PATH에 `g++`, `clang++`, `ninja` 중 필요한 도구 추가
- generator를 명시해서 실행: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`

Windows에서 Visual Studio/MSVC generator를 쓰면 실행 파일은 보통 아래 위치에 생깁니다.

```powershell
.\build\Release\tiny_rag.exe --query "C++17 llama.cpp GGUF quantization" --emit-prompt prompt.txt
.\build\Release\sampling_demo.exe --temperature 0.8 --top-k 5 --seed 42
```

Ninja, MinGW, Linux, macOS 같은 single-config generator에서는 보통 아래처럼 실행합니다.

```powershell
.\build\tiny_rag.exe --query "C++17 llama.cpp GGUF quantization" --emit-prompt prompt.txt
.\build\sampling_demo.exe --temperature 0.8 --top-k 5 --seed 42
```

## 다음 단계: 실제 llama.cpp로 연결

```powershell
cd D:\workspace\RoboticsStudy\Opensource
git clone https://github.com/ggml-org/llama.cpp.git
cd llama.cpp
cmake -B build
cmake --build build --config Release
```

모델 파일은 포함하지 않았습니다. GGUF 모델은 모델 라이선스를 직접 확인한 뒤 별도로 받아야 합니다.

Windows Release 빌드 기준 실행 예시는 다음과 같습니다.

```powershell
.\build\bin\Release\llama-cli.exe -m .\models\your-model.gguf -p "Explain C++17 move semantics in Korean." -n 128
```

## 학습 순서

1. `docs/01_open_source_choice.md`로 왜 `llama.cpp`가 적합한지 확인합니다.
2. `docs/02_llama_cpp_learning_guide.md`의 주차별 로드맵을 따라갑니다.
3. `code/src/tiny_rag.cpp`와 `code/src/sampling_demo.cpp`를 빌드하고 수정합니다.
4. 같은 개념을 `llama.cpp`의 tokenizer, GGUF, sampling, backend 코드와 연결해서 읽습니다.
