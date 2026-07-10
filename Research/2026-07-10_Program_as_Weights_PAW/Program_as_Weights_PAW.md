# Program-as-Weights: Fuzzy Function을 위한 새로운 프로그래밍 패러다임

> 논문: **Program-as-Weights: A Programming Paradigm for Fuzzy Functions**  
> arXiv: 2607.02512v1  
> 저자: Wentao Zhang, Liliana Hotsko, Woojeong Kim, Pengyu Nie, Stuart Shieber, Yuntian Deng  
> 기관: University of Waterloo, Cornell University, Harvard University  
> 공개일: 2026-07-02  
> 핵심 키워드: Program-as-Weights, PAW, fuzzy functions, neural compiler, PEFT, LoRA, local execution

## 한 줄 요약

이 논문은 자연어로 설명되는 애매한 함수, 즉 **fuzzy function**을 매번 대형 LLM API에 맡기는 대신, 한 번 **컴파일**해서 작은 neural program으로 만들고 이후에는 로컬의 작은 interpreter가 반복 실행하게 하자는 제안이다.

```text
기존 방식:
    매 입력마다 cloud LLM 호출

PAW 방식:
    함수 명세를 한 번 compile -> 작은 weight artifact 생성 -> 로컬 interpreter가 반복 실행
```

논문은 이 방식을 **Program-as-Weights(PAW)**라고 부른다.

## 왜 이 논문을 봐야 하나

실무 코드에는 명확한 규칙으로 쓰기 어려운 함수가 많다.

- 로그 라인이 중요한지 판단하기
- 깨진 JSON을 복구하기
- 검색 결과가 사용자의 의도와 맞는지 재랭킹하기
- 사용자의 자연어 명령을 라벨로 분류하기
- 도구 호출 전처리하기

이런 함수는 정규식이나 if-else로 만들면 취약하다. 그래서 요즘은 다음처럼 LLM API를 직접 함수처럼 부르는 코드가 많아진다.

```python
result = llm("Classify whether this message is urgent", message)
```

편하지만 문제가 있다.

- 호출마다 비용이 든다.
- 네트워크와 외부 API에 의존한다.
- 모델 제공자가 업데이트하면 재현성이 흔들린다.
- 로컬/offline 실행이 어렵다.
- 짧고 반복적인 fuzzy function에도 과도하게 큰 모델을 매번 쓴다.

PAW는 이 비용 구조를 바꾸려는 시도다.

```text
큰 모델은 compile time에만 사용하고,
runtime에는 작은 모델 + 작은 adapter만 사용한다.
```

## 초록 의미 중심 번역

일상적인 프로그래밍 작업 중에는 규칙 기반 구현으로 깔끔하게 처리하기 어려운 문제가 많다. 예를 들어 중요한 로그 라인만 알림으로 보내기, 깨진 JSON 복구하기, 검색 결과를 의도에 따라 정렬하기 같은 작업이다. 이런 작업은 대형 언어 모델 API에 맡겨지고 있지만, 이는 locality, reproducibility, cost 측면의 대가를 치른다.

저자들은 fuzzy-function programming을 제안한다. 자연어 명세로부터 compact하고 local execution 가능한 neural artifact를 컴파일하는 방식이다. 구체 구현인 PAW에서는 4B compiler가 FuzzyBench라는 1천만 예제 데이터셋으로 학습되어, frozen lightweight interpreter를 위한 parameter-efficient adapter를 생성한다. 0.6B Qwen3 interpreter가 PAW program을 실행하면 Qwen3-32B 직접 prompting과 비슷하거나 더 나은 성능을 내면서, inference memory는 약 1/50 수준이고 MacBook M3에서 30 tokens/s 수준으로 동작한다.

PAW는 foundation model을 per-input solver가 아니라 tool builder로 재정의한다. 함수 정의 시 한 번 호출되어 작은 재사용 artifact를 만들고, 이후 함수 적용은 cheap하고 offline으로 수행된다.

## 핵심 개념 1: Fuzzy Function

논문에서 말하는 fuzzy function은 사람이 보면 직관적인데, 엄밀한 symbolic rule로 쓰기 어려운 함수다.

예:

```text
입력: "Need your signature by EOD!"
출력: "urgent"
```

이 함수는 규칙으로 만들 수는 있다.

```python
if "EOD" in text or "urgent" in text:
    return "urgent"
```

하지만 현실 입력은 곧 깨진다.

```text
"Could you sign this before the board packet goes out?"
"Client is blocked unless we confirm today."
"No rush, just for your reference."
```

키워드만으로는 부족하다. 의미, 맥락, 암시를 읽어야 한다. 이 때문에 LLM이 잘 맞지만, 매번 큰 LLM을 호출하는 것은 소프트웨어 함수로는 비싸다.

## 핵심 개념 2: Program-as-Weights

PAW의 기본 수식은 다음처럼 볼 수 있다.

```text
program p = Compiler(specification s)
output y = Interpreter(program p, input x)
```

일반 프로그래밍과 대응시키면 다음과 같다.

| 전통 프로그래밍 | PAW |
|---|---|
| source code | natural-language specification |
| compiler | neural compiler |
| binary / library | pseudo-program + PEFT weights |
| runtime / VM | frozen neural interpreter |
| function call | interpreter with loaded PAW program |

중요한 점은 interpreter가 고정되어 있다는 것이다. 새로운 fuzzy function이 생길 때마다 interpreter를 다시 학습하지 않는다. 대신 새로운 program artifact만 컴파일한다.

## 핵심 개념 3: Hybrid Program

논문에서 PAW program은 두 부분으로 구성된다.

```text
program = discrete pseudo-program + continuous PEFT module
```

### Discrete pseudo-program

자연어 명세를 더 깨끗하게 정리한 설명과 입출력 예시다. noisy한 사용자 명세를 정돈해 interpreter가 보기 쉽게 만든다.

예:

```text
Task: Classify message urgency.
Return only: urgent or normal.
Input: "Need your signature by EOD"
Output: urgent
```

### Continuous PEFT module

LoRA나 prefix-tuning 같은 작은 weight adapter다. 텍스트 설명만으로 전달하기 어려운 미세한 행동을 interpreter 내부에 주입한다.

논문 최신 구현은 **Text-to-LoRA**다. compiler가 명세와 pseudo-program을 읽고, frozen interpreter에 붙일 LoRA weight를 생성한다.

## Compiler-Interpreter 구조

PAW pipeline은 세 부분이다.

```text
1. Pseudo compiler
   raw natural-language spec -> clean pseudo-program

2. PEFT compiler
   spec + pseudo-program -> LoRA adapter

3. Frozen interpreter
   pseudo-program + LoRA + user input -> output
```

논문 구현:

- pseudo compiler: off-the-shelf Qwen3-4B-Instruct-2507, 학습하지 않음
- LoRA compiler: Qwen3-4B 기반, 학습됨
- interpreter: frozen Qwen3-0.6B 또는 Qwen3.5-0.8B 등
- program size: Qwen3-0.6B 기준 quantized LoRA adapter 약 23MB

## Text-to-LoRA의 직관

LoRA는 큰 weight matrix `W`에 작은 low-rank update를 더하는 방식이다.

```text
W' = W + ΔW
ΔW = B A
```

여기서 `A`, `B`는 작다. 보통 LoRA는 특정 task 데이터로 직접 학습한다. PAW는 다르다.

```text
일반 LoRA:
    task data로 adapter를 gradient descent 학습

PAW:
    자연어 spec을 compiler가 읽고 adapter를 즉시 생성
```

즉, compiler가 "이런 함수라면 이런 LoRA가 필요하다"를 학습해 둔 hypernetwork처럼 동작한다.

논문에서는 compiler hidden state를 LoRA mapper가 받아 layer/module별 mixing coefficients를 만들고, shared basis LoRA를 조합해 adapter를 생성한다.

## FuzzyBench

PAW를 학습하려면 다음 형태의 데이터가 필요하다.

```text
(specification, input, target output)
```

논문은 이를 위해 **FuzzyBench-10M**을 만든다.

- 총 1천만 예제
- 29개 incremental version
- 800개 이상 fuzzy task category
- classification, parsing, fuzzy matching, format conversion, tool use, web intelligence 등 포함
- test specification은 training specification과 분리
- noisy spec robustness 평가용 변형 포함

중요한 점은 예제가 input/output만 있는 것이 아니라, "어떤 fuzzy function을 컴파일해야 하는지"를 알려주는 specification을 포함한다는 것이다.

## 학습 목표

학습되는 것은 PEFT compiler다. pseudo compiler와 interpreter는 고정된다.

각 학습 예제에서:

```text
spec -> pseudo-program 조회
spec + pseudo-program -> compiler forward
compiler hidden states -> LoRA mapper -> LoRA
LoRA를 frozen interpreter에 attach
input을 넣고 target output likelihood 최대화
```

손실은 target output의 negative log likelihood다.

```text
L = - mean log p_interpreter(target | input, pseudo-program, generated LoRA)
```

이 구조에서 gradient는 frozen interpreter의 출력 손실에서 LoRA mapper와 compiler hidden state 쪽으로 흐르지만, interpreter parameter 자체는 업데이트하지 않는다.

## 주요 결과

논문의 핵심 숫자는 다음이다.

| 방법 | FuzzyBench 정확도 | 특징 |
|---|---:|---|
| Qwen3 0.6B 직접 prompting | 9.84% | 작은 모델은 spec만 보고 fuzzy function 수행이 어려움 |
| Qwen3 32B 직접 prompting | 68.70% | 큰 모델 API/로컬 대형 모델 필요 |
| PAW Qwen3 0.6B | 73.78% | 작은 interpreter + per-program adapter |
| gpt-5.2 API | 96.09% | 데이터 생성 모델, empirical ceiling |
| gpt-5-mini API | 91.87% | 독립 strong verifier 역할 |

논문은 PAW Qwen3-0.6B가 Qwen3-32B direct prompting보다 높은 정확도를 보이면서, bf16 기준 inference memory는 약 1.2GB vs 60GB라고 보고한다.

## 중요한 ablation

### Prefix-tuning vs LoRA

동일한 compute scale에서:

- prompting baseline: 9.8%
- prefix tuning: 50.4%
- text-to-LoRA: 56.5% 또는 default 설정 65.7%

LoRA가 더 강한 PEFT 형태로 확인되었다.

### Compiler가 꼭 필요한가

동일한 0.6B base에서:

- fixed LoRA: 최대 52.10%
- full fine-tuning: 58.40%
- PAW: 73.78%

즉, 성능 향상은 단순히 작은 모델을 fine-tuning해서가 아니라, **명세별 LoRA를 생성하는 compiler**에서 온다.

### noisy specification에 강한 이유

논문은 pseudo-program이 denoising 역할을 한다고 본다. raw spec에 typo나 formatting drift가 있어도, pseudo compiler가 clean restatement를 만들어 small interpreter가 안정적으로 실행한다.

heavy typo 조건에서 pseudo-program을 쓰는 쪽이 raw spec만 쓰는 쪽보다 약 4.5 point 높게 보고된다.

## Local Execution

논문이 실무적으로 중요한 부분이다.

개발자 사용 흐름은 다음과 같다.

```python
program = paw.compile(spec, slug="email-triage")
fn = paw.function("email-triage")
print(fn("Need your signature today."))
```

compile은 cloud에서 한 번 수행될 수 있지만, 이후 실행은 로컬 interpreter가 담당한다.

논문은 Qwen3 0.6B quantized base와 per-program LoRA adapter를 GGUF로 만들었고, MacBook M3에서 약 31.6 tokens/s를 보고한다. base는 공유되고, 각 fuzzy function은 작은 adapter 파일로 배포된다.

## 멀티모달 확장

재미있는 점은 interpreter를 바꾸지 않고도 image-conditioned fuzzy function을 실험했다는 것이다.

구조:

```text
text-only compiler -> Qwen3-VL compiler로 교체
interpreter는 같은 Qwen3 0.6B 유지
image information은 compiler가 만든 PEFT module 안에 압축됨
```

즉, 작은 text interpreter는 픽셀을 직접 보지 않는다. image-conditioned task 정보가 adapter에 들어간다.

이 관점은 "멀티모달 정보를 runtime input으로 매번 넣는 대신, compile-time에 task adapter로 압축할 수 있는가"라는 질문을 던진다.

## 왜 써야 하는가

PAW를 써야 하는 이유는 정확도 하나가 아니다. 비용 구조와 소프트웨어 배포 방식이 바뀐다.

### 1. 반복 호출 비용 절감

같은 fuzzy function을 수천, 수만 번 호출한다면 매번 큰 LLM API를 부르는 것은 낭비다. PAW는 compile time 비용을 한 번 내고, runtime 비용을 작게 만든다.

### 2. 로컬 실행과 offline 가능성

compiled program과 interpreter가 있으면 네트워크 없이 실행할 수 있다. 로봇, 엣지 장비, 보안 환경, 사내망에서 유리하다.

### 3. 재현성

LLM API는 제공자 업데이트로 행동이 바뀔 수 있다. PAW program은 versioned artifact로 저장할 수 있다.

### 4. 함수 단위 배포

Python module처럼 `email_triage.paw`, `json_repair.paw` 같은 함수 artifact를 배포하는 모델이다. 자연어 명세가 소스 코드이고, adapter가 binary에 해당한다.

### 5. 작은 모델 미래

모든 입력마다 거대 모델을 호출하는 대신, 큰 모델은 compiler 역할만 하고 작은 runtime이 대부분의 호출을 처리한다. 소프트웨어 아키텍처 관점에서 의미가 크다.

## 한계

논문도 명확한 한계를 인정한다.

- compiler와 interpreter가 강하게 결합된다. interpreter를 바꾸면 compiler 재학습이 필요하다.
- continuous PEFT component는 해석하기 어렵다.
- 평가가 대부분 single-step fuzzy function이다.
- multi-step/long-horizon reasoning은 아직 검증되지 않았다.
- FuzzyBench는 synthetic data이므로 외부 실사용 검증이 더 필요하다.
- task에 따라 LoRA가 좋은지 prefix-tuning이 좋은지 아직 원칙적 기준이 없다.

## 실습 프로젝트

이 폴더의 [paw_toy_project](./paw_toy_project)는 PAW 개념을 작은 코드로 재현한다.

실제 LoRA나 Transformer는 사용하지 않는다. 대신 다음 대응을 사용한다.

| 논문 PAW | toy project |
|---|---|
| natural-language spec | `spec` 문자열 |
| pseudo compiler | spec을 정규화하고 예시를 만드는 함수 |
| continuous LoRA | keyword weight dictionary |
| frozen interpreter | 공통 scoring runtime |
| PAW program file | JSON 파일 |

실행:

```powershell
python Research\2026-07-10_Program_as_Weights_PAW\paw_toy_project\demo.py
```

실행하면 다음이 생성된다.

- `programs/email_triage.paw.json`
- `programs/log_monitor.paw.json`
- `programs/search_rerank.paw.json`

그리고 같은 interpreter가 세 프로그램을 번갈아 load해서 실행한다. 핵심은 "규칙 코드를 매번 새로 작성하는 것"이 아니라 "spec을 작은 program artifact로 compile하고 runtime에서 hot-load한다"는 구조를 이해하는 것이다.

## 실무 적용 아이디어

- CI 로그에서 중요한 이벤트만 alert
- 사내 검색 결과 semantic reranking
- 고객 문의 intent classification
- 민감 정보 제거/마스킹
- 도구 호출 전 라우팅
- 로봇 실험 로그의 실패 원인 triage

PAW식 구조를 그대로 구현하려면 대규모 compiler 학습이 필요하지만, 제품 설계 관점에서는 이미 적용할 수 있는 교훈이 있다.

```text
자주 반복되는 fuzzy task는 prompt 호출로 방치하지 말고,
작은 versioned artifact로 고정하고 로컬 실행 경로를 설계하라.
```

## 참고 링크

- arXiv HTML: https://arxiv.org/html/2607.02512v1
- arXiv abstract: https://arxiv.org/abs/2607.02512
- arXiv PDF: https://arxiv.org/pdf/2607.02512
- GitHub organization: https://github.com/programasweights
- Demo site: https://programasweights.com
