# 오픈소스 선정: llama.cpp

## 결론

C++17 이상의 지식과 AI를 같이 익히려면 `ggml-org/llama.cpp`가 가장 실용적인 선택입니다.

`llama.cpp`는 로컬 환경에서 LLM을 추론하기 위한 C/C++ 기반 오픈소스입니다. 단순히 API를 호출하는 프로젝트가 아니라, 모델 파일 로딩, 텐서 연산, 토큰화, 샘플링, CPU/GPU 최적화, 서버 모드까지 포함합니다. 그래서 C++ 프로젝트 구조와 AI 추론 파이프라인을 동시에 볼 수 있습니다.

공식 링크:

- GitHub: https://github.com/ggml-org/llama.cpp
- Build guide: https://github.com/ggml-org/llama.cpp/blob/master/docs/build.md
- License: https://github.com/ggml-org/llama.cpp/blob/master/LICENSE

## 왜 C++17+ 학습에 좋은가

- CMake 기반 대형 프로젝트 구조를 익힐 수 있습니다.
- `target_compile_features(... cxx_std_17)`와 `CMAKE_CXX_STANDARD 17` 계열 설정을 실제 프로젝트에서 확인할 수 있습니다.
- CLI 도구, 라이브러리, 예제, 테스트, 서버가 나뉘어 있어 모듈 경계를 읽기 좋습니다.
- 성능 중심 코드라서 메모리 배치, 캐시, 스레딩, SIMD, GPU backend 같은 저수준 주제를 접할 수 있습니다.
- AI 관점에서는 GGUF, quantization, context, batch, logits, sampling, embeddings 같은 LLM 실무 용어를 코드와 같이 배울 수 있습니다.

## 다른 후보와 비교

| 후보 | 장점 | 단점 | 추천도 |
| --- | --- | --- | --- |
| llama.cpp | LLM 추론 전체 흐름, C/C++ 성능 코드, CMake, 로컬 실행 | 코드량이 많아 초반 진입 장벽 있음 | 가장 추천 |
| ONNX Runtime | 다양한 모델 포맷과 production inference 이해에 좋음 | 프로젝트 규모가 매우 크고 학습 초점이 분산됨 | 중급 이후 |
| OpenCV DNN | 컴퓨터비전 AI와 C++를 같이 학습하기 좋음 | LLM/생성형 AI보다는 전통 CV 추론 중심 | CV 목표라면 추천 |
| TensorFlow Lite | 모바일/임베디드 추론 이해에 좋음 | 빌드와 의존성이 초보자에게 무거울 수 있음 | 목적이 명확할 때 |

## llama.cpp에서 먼저 읽을 위치

처음부터 모든 파일을 읽지 말고 아래 순서로 좁게 들어가는 것이 좋습니다.

1. `README.md`: 프로젝트 목표와 사용법 파악
2. `docs/build.md`: 빌드 옵션과 backend 종류 파악
3. `include/llama.h`, `include/llama-cpp.h`: 외부 API 표면 파악
4. `examples/`: API 사용 예시 확인
5. `common/`: CLI 공통 옵션, 샘플링, 프롬프트 처리 흐름 확인
6. `ggml/`: 텐서 연산과 backend 구조 확인
7. `tools/server/`: LLM을 서비스로 노출하는 구조 확인

## 이 폴더의 예제가 하는 역할

`code/` 아래 예제는 실제 LLM을 직접 구현하지 않습니다. 대신 LLM 애플리케이션에서 자주 만나는 두 개념을 C++17만으로 작게 재현합니다.

- `tiny_rag.cpp`: 문서 검색, TF-IDF, cosine similarity, 프롬프트 생성
- `sampling_demo.cpp`: logits, softmax, temperature, top-k sampling

이 두 예제를 이해하면 `llama.cpp`의 tokenizer, embedding/RAG 활용, logits 후처리, sampling 코드를 읽을 때 연결 지점이 생깁니다.
