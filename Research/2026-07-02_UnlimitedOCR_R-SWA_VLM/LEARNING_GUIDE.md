# 학습 가이드: R-SWA와 Tool-Guided VLM 직접 실험하기

이 가이드는 실제 대형 VLM을 내려받지 않고도 오늘 정리한 핵심 아이디어를 손으로 확인하는 데 목적이 있다.

## 준비

필요한 것은 Python 표준 라이브러리뿐이다.

```powershell
python --version
```

Python 3.10 이상이면 충분하다.

## 1단계: R-SWA 메모리 증가 비교

실행:

```powershell
python Research\2026-07-02_UnlimitedOCR_R-SWA_VLM\rswa_memory_demo.py
```

관찰할 것:

- Full attention은 생성 토큰 수가 늘수록 cache 크기가 계속 증가한다.
- R-SWA는 reference token 수와 sliding window 크기만 유지하므로 cache가 일정 수준에서 멈춘다.
- output token 수가 1,024에서 16,384로 늘어날 때 두 방식의 차이가 급격히 커진다.

확장 과제:

- `reference_tokens` 값을 256, 1024, 4096으로 바꿔본다.
- `window_size` 값을 64, 128, 512로 바꿔본다.
- 표준 attention과 R-SWA의 차이를 그래프로 그리고 싶다면 matplotlib을 추가해도 된다.

## 2단계: Tool-Guided Visual Registry 이해하기

실행:

```powershell
python Research\2026-07-02_UnlimitedOCR_R-SWA_VLM\tool_guided_registry_demo.py
```

관찰할 것:

- 원본 숫자 이미지를 바로 판단하지 않고, crop, contrast, line overlay 같은 도구 결과를 registry에 계속 저장한다.
- 각 도구 결과는 새로운 id를 가진 immutable resource로 취급한다.
- 모델이 "원본을 봤다"에서 멈추지 않고 "증거 이미지를 만들어 다시 확인한다"는 흐름이 생긴다.

확장 과제:

- `crop()` 범위를 바꿔서 관심 영역을 바꿔본다.
- `threshold()` 값을 조절해서 미세한 밝기 차이가 사라지거나 살아나는지 확인한다.
- `draw_vertical_line()` 대신 horizontal line 도구를 직접 만들어본다.

## 3단계: 실제 연구와 연결하기

R-SWA:

- 실제 모델에서는 token embedding마다 key/value tensor가 있으므로 메모리는 단순 token 개수보다 훨씬 크다.
- 하지만 증가 형태는 같다. Full attention은 출력 길이에 비례하고, R-SWA는 window 크기에 의해 제한된다.

Tool-Guided VLM:

- 실제 논문에서는 VLM이 도구를 호출하고 결과 이미지를 다시 읽는다.
- 이 예제는 VLM 대신 사람이 registry를 읽는다. 핵심은 도구 결과를 추론 체인 안에 명시적으로 남기는 구조다.

Perceval:

- 직접 구현하려면 먼저 모델 답변에서 이미지 관련 claim을 추출해야 한다.
- 그 다음 claim이 이미지 증거와 맞는지 판정하는 verifier가 필요하다.
- 작은 버전으로는 "답변 문장 -> claim list -> rule 기반 검증"부터 시작할 수 있다.

## 추천 학습 순서

1. `2026-07-02_UnlimitedOCR_R-SWA_VLM_Top5.md`를 먼저 읽는다.
2. `rswa_memory_demo.py`를 실행하고 숫자 차이를 확인한다.
3. `tool_guided_registry_demo.py`를 실행하고 registry 출력 흐름을 따라간다.
4. 각 스크립트의 파라미터를 바꿔 결과가 어떻게 변하는지 확인한다.
5. 이후 실제 논문 PDF의 method section을 읽으면 수식과 코드가 더 잘 연결된다.
