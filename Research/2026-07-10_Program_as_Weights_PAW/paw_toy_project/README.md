# PAW Toy Project

`Program-as-Weights` 논문의 핵심 추상화를 작은 Python 프로젝트로 재현합니다.

실제 구현과의 차이:

- 실제 PAW는 neural compiler가 LoRA adapter를 생성합니다.
- 이 toy project는 LoRA 대신 keyword weight dictionary를 생성합니다.
- 실제 PAW는 frozen neural interpreter를 사용합니다.
- 이 toy project는 공통 scoring interpreter를 사용합니다.

그래도 핵심 구조는 같습니다.

```text
spec -> compile -> .paw.json program -> local interpreter run
```

## 실행

```powershell
python Research\2026-07-10_Program_as_Weights_PAW\paw_toy_project\demo.py
```

## 파일

- `paw_toy.py`: compiler/interpreter 구현
- `demo.py`: 세 fuzzy function compile/run 데모
- `programs/*.paw.json`: 실행 시 생성되는 compiled program artifacts

## 관찰할 것

1. natural-language spec이 pseudo-program과 weights로 바뀝니다.
2. 생성된 program file은 JSON이라 저장/버전관리/재사용 가능합니다.
3. 같은 interpreter가 여러 program을 hot-load해서 다른 fuzzy function처럼 실행합니다.
4. compile과 run이 분리되어 있습니다.
