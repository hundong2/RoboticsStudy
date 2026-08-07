# llama.cpp 학습 가이드

## 준비물

Windows 기준 권장 도구:

- Git
- CMake 3.14 이상
- Visual Studio 2022의 "Desktop development with C++" workload 또는 clang/g++ toolchain
- 선택 사항: NVIDIA CUDA, Vulkan SDK, AMD ROCm/HIP

현재 워크스페이스에서는 CMake는 확인되었지만 `g++`, `clang++`, `cl`은 PATH에서 확인되지 않았습니다. Visual Studio가 설치되어 있다면 "x64 Native Tools Command Prompt for VS" 또는 "Developer PowerShell for VS"에서 다시 빌드하면 됩니다.

이 환경에서 확인한 실패 메시지:

- 기본 generator: `NMake Makefiles`
- 실패 원인: `nmake`를 찾지 못함
- `Visual Studio 17 2022` generator 확인 결과: 설치된 Visual Studio 인스턴스를 찾지 못함

해결 방법은 Visual Studio C++ workload를 설치하거나, MinGW-w64/LLVM/Ninja toolchain을 설치하고 PATH를 다시 여는 것입니다.

## 1단계: 이 폴더의 미니 프로젝트 빌드

```powershell
cd D:\workspace\RoboticsStudy\Opensource\code
cmake -S . -B build
cmake --build build --config Release
```

실행:

```powershell
.\build\Release\tiny_rag.exe --query "C++17 llama.cpp GGUF quantization" --emit-prompt prompt.txt
.\build\Release\sampling_demo.exe --temperature 0.9 --top-k 4 --seed 7
```

single-config generator라면:

```powershell
.\build\tiny_rag.exe --query "C++17 llama.cpp GGUF quantization" --emit-prompt prompt.txt
.\build\sampling_demo.exe --temperature 0.9 --top-k 4 --seed 7
```

## 2단계: llama.cpp 다운로드와 CPU 빌드

```powershell
cd D:\workspace\RoboticsStudy\Opensource
git clone https://github.com/ggml-org/llama.cpp.git
cd llama.cpp
cmake -B build
cmake --build build --config Release
```

빌드가 끝나면 Windows Release 기준으로 `build\bin\Release` 아래에 `llama-cli.exe`, `llama-server.exe` 같은 실행 파일이 생성됩니다.

## 3단계: 모델 준비

`llama.cpp`는 보통 GGUF 모델 파일을 사용합니다. 모델은 이 저장소나 이 예제에 포함하지 않습니다.

모델을 받을 때 확인할 것:

- 모델 라이선스가 개인/상업/연구 목적에 맞는지
- GGUF 파일인지
- 로컬 메모리와 VRAM에 맞는 크기인지
- quantization 종류가 무엇인지

처음에는 1B-4B급 소형 instruction 모델의 `Q4_K_M` 또는 `Q8_0` 계열이 실습에 부담이 적습니다.

## 4단계: 첫 추론 실행

```powershell
.\build\bin\Release\llama-cli.exe -m .\models\your-model.gguf -p "C++17 structured binding을 한국어로 설명해줘." -n 128
```

대화 모드:

```powershell
.\build\bin\Release\llama-cli.exe -m .\models\your-model.gguf -cnv
```

서버 모드:

```powershell
.\build\bin\Release\llama-server.exe -m .\models\your-model.gguf
```

## 5단계: GPU backend 옵션

CPU 빌드가 먼저 성공한 뒤 GPU를 붙이는 것이 좋습니다.

CUDA:

```powershell
cmake -B build-cuda -DGGML_CUDA=ON
cmake --build build-cuda --config Release
```

Vulkan:

```powershell
cmake -B build-vulkan -DGGML_VULKAN=ON
cmake --build build-vulkan --config Release
```

GPU 실행에서는 `-ngl` 옵션으로 GPU에 올릴 layer 수를 조정합니다.

```powershell
.\build-vulkan\bin\Release\llama-cli.exe -m .\models\your-model.gguf -p "Hello" -ngl 99
```

## 6주 학습 로드맵

| 주차 | C++17 주제 | AI/LLM 주제 | 실습 |
| --- | --- | --- | --- |
| 1 | CMake, target, warning option | LLM 추론 흐름 | 이 폴더 예제 빌드 |
| 2 | `std::filesystem`, `std::optional`, structured binding | 문서 검색, RAG | `tiny_rag.cpp` 수정 |
| 3 | RAII, move semantics, ownership | 모델 파일, GGUF, tokenizer | llama.cpp CLI 실행 |
| 4 | algorithm, numeric, random | logits, softmax, top-k, temperature | `sampling_demo.cpp` 수정 |
| 5 | profiling, memory layout, threading | quantization, CPU/GPU backend | CPU와 GPU 빌드 비교 |
| 6 | API 경계, 테스트, 작은 PR | 서버, embeddings, integration | llama-server와 RAG 연결 |

## 추천 과제

1. `tiny_rag.cpp`에 stopword 제거를 추가합니다.
2. `sampling_demo.cpp`에 top-p nucleus sampling을 추가합니다.
3. `tiny_rag`가 생성한 `prompt.txt`를 `llama-cli` 입력으로 사용합니다.
4. `llama.cpp`의 `common/` 디렉터리에서 샘플링 관련 코드를 찾아 이 예제와 비교합니다.
5. `docs/build.md`의 backend 옵션 중 현재 PC에서 가능한 것을 하나 골라 빌드합니다.
