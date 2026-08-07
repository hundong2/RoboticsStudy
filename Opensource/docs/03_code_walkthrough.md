# 예제 코드 설명

## tiny_rag.cpp

`tiny_rag.cpp`는 RAG의 가장 작은 형태를 보여줍니다.

흐름:

1. `data/knowledge_base.txt`에서 문서를 읽습니다.
2. 문장을 토큰으로 나눕니다.
3. 각 문서를 TF-IDF sparse vector로 바꿉니다.
4. 사용자 query도 같은 방식으로 vector화합니다.
5. cosine similarity로 관련 문서를 정렬합니다.
6. 상위 문서를 모아 LLM에 넣을 프롬프트 파일을 만듭니다.

사용한 C++17 요소:

- `std::filesystem::path`
- `std::optional`
- structured binding
- lambda
- standard algorithms
- `std::unordered_map` 기반 sparse vector

실행 예:

```powershell
cd D:\workspace\RoboticsStudy\Opensource\code
.\build\Release\tiny_rag.exe --query "C++17 llama.cpp quantization" --top-k 3 --emit-prompt prompt.txt
```

생성된 `prompt.txt`는 실제 LLM CLI에 넘길 수 있는 형태입니다.

```powershell
.\..\llama.cpp\build\bin\Release\llama-cli.exe -m .\..\llama.cpp\models\your-model.gguf -f prompt.txt -n 256
```

## sampling_demo.cpp

`sampling_demo.cpp`는 LLM이 다음 token을 고르는 방식을 단순화한 예제입니다.

흐름:

1. token별 logit 값을 준비합니다.
2. temperature를 적용합니다.
3. top-k token만 남깁니다.
4. softmax로 확률을 만듭니다.
5. `std::discrete_distribution`으로 token을 샘플링합니다.

사용한 C++17 요소:

- structured binding
- `std::mt19937`
- `std::discrete_distribution`
- `std::clamp`
- `std::sort`, `std::max_element`, `std::accumulate`

실행 예:

```powershell
.\build\Release\sampling_demo.exe --temperature 0.7 --top-k 5 --seed 123
```

## llama.cpp와의 연결

| 이 예제 | llama.cpp에서 연결되는 개념 |
| --- | --- |
| tokenization | tokenizer, prompt processing |
| TF-IDF vector | embedding/RAG의 기본 아이디어 |
| cosine similarity | embedding search, context selection |
| prompt.txt 생성 | `llama-cli -f`, server request body |
| logits | model output before token selection |
| softmax | probability conversion |
| temperature, top-k | decoding/sampling strategy |

## 수정 연습

1. `tiny_rag.cpp`에서 한국어 조사 처리를 개선합니다.
2. `knowledge_base.txt`에 직접 llama.cpp 학습 노트를 추가합니다.
3. `sampling_demo.cpp`에 `--steps` 옵션을 추가해 생성 길이를 바꿉니다.
4. `sampling_demo.cpp`에 top-p sampling을 추가합니다.
5. `tiny_rag.cpp`가 JSON 형태로 결과를 출력하도록 바꿉니다.
