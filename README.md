# RoboticsStudy

## Todo 

- [ ] [LLM/VLM 용어집](./Research/용어집/README.md)

- [ ] [CNN 및 관련 기술](./ETC/README.md)  
- [ ] [HOG](./opencv4/08.MachineLearning.md)     
- [ ] [Research](./Research/README.md)  
- [ ] [Jetson Orin Nano Edge AI Productization](./Research/2026-07-02_Jetson_Edge_AI_Productization/README.md)
- [ ] [Darknet YOLO 정리 및 실습](./ETC/tech/darknet.md)


> OpenCV를 Python 및 C#(.NET)으로 학습하기 위한 스터디 저장소입니다.  
> *"C#과 파이썬을 활용한 OpenCV4 프로그래밍"* 교재 예제 실습 및 개인 실험 코드를 포함합니다.

## LLM/VLM 용어집 파일 목록

- [용어집 README](./Research/용어집/README.md)
- [01. LLM 기본 용어](./Research/용어집/01_llm_core.md)
- [02. Attention과 Transformer 용어](./Research/용어집/02_attention_transformer.md)
- [03. KV Cache와 추론 시스템 용어](./Research/용어집/03_kv_cache_inference.md)
- [04. 학습, 튜닝, 정렬 용어](./Research/용어집/04_training_finetuning.md)
- [05. 효율화, 병렬화, 서빙 용어](./Research/용어집/05_efficiency_systems.md)
- [06. 평가 지표와 벤치마크 용어](./Research/용어집/06_evaluation_benchmarks.md)
- [07. VLM과 멀티모달 용어](./Research/용어집/07_multimodal_vlm.md)
- [08. 논문 독해용 학습 전략](./Research/용어집/08_paper_reading_strategy.md)

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Python](https://img.shields.io/badge/Python-3.13%2B-blue.svg)
![.NET](https://img.shields.io/badge/.NET-10-purple.svg)
![OpenCV](https://img.shields.io/badge/OpenCV-4.x-green.svg)

---

## 📁 Repository Structure

```
RoboticsStudy/
├── opencv4/                    # 교재 예제 코드 (Chapter1 ~ Chapter10)
│   ├── Chapter1/               # OpenCV 기초, 환경 구성
│   ├── Chapter2/               # 이미지 입출력
│   ├── Chapter3/               # 이미지 처리 기초
│   ├── ...
│   └── Chapter10/              # 딥러닝 실전 예제 (SSD, COCO 등)
├── OpenCVStudyForPython/       # Python 독립 실습 환경 (uv 기반)
│   ├── StartCV/                # 버전 확인, 기본 예제
│   ├── main.py
│   └── pyproject.toml
├── OpenCVStudyForDotnet/       # C#(.NET) 독립 실습 환경
│   ├── StartCV/                # 버전 확인, 기본 예제
│   └── OpenCVTest/
├── opencvsharp_samples/        # OpenCvSharp 공식 샘플 (서브모듈)
├── OpenCV/                     # OpenCV 개념 정리 노트
├── environment/                # VS Code 환경 설정 파일
├── run_env.sh                  # Python venv 활성화 스크립트
└── LICENSE
```

---

## ⚙️ Prerequisites

### Python 환경

| 항목 | 버전 |
|------|------|
| Python | 3.13 이상 |
| OpenCV | 4.13.x 이상 |
| 패키지 매니저 | [uv](https://github.com/astral-sh/uv) |

### .NET 환경

| 항목 버전 |
|----------|
| .NET SDK 10 |
| dotnet-interactive |

### VS Code 확장

- **Python** — Python 언어 지원
- **Jupyter** — Notebook 실행
- **C# Dev Kit** — C# 언어 지원
- **Polyglot Notebooks** — C# Notebook 실행

---

## 🚀 Getting Started

### Python 환경 설정

```bash
# 저장소 클론
git clone https://github.com/hundong2/RoboticsStudy.git
cd RoboticsStudy/OpenCVStudyForPython

# 의존성 설치 (uv 사용)
uv sync

# 가상환경 활성화
source ../.venv/bin/activate
# 또는 프로젝트 루트에서
source run_env.sh   # 반드시 source 명령으로 실행
```

> **주의:** `./run_env.sh`로 직접 실행하면 서브셸에서 동작하여 현재 터미널에 venv가 적용되지 않습니다.  
> 반드시 `source run_env.sh` 또는 `. run_env.sh`로 실행하세요.

```bash
# 패키지 추가 시
uv add <패키지명>

# 가상환경 비활성화
deactivate
```

### .NET 환경 설정

Ubuntu 22.04 / WSL2 기준:

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
dotnet interactive --version
```

---

## 📖 교재 예제 실행 방법

### Python 예제

```bash
cd opencv4/Chapter3/Example-03-09-Py
python main.py
```

### C# 예제 (Visual Studio)

1. `opencv4/Chapter1/Example-01-01-C#` 폴더의 솔루션 파일(`.sln`)을 Visual Studio로 엽니다.
2. `Ctrl + F5`로 **디버깅 없이 실행**합니다.

### C# Notebook (VS Code)

1. `OpenCVStudyForDotnet/StartCV/` 폴더의 `.ipynb` 파일을 엽니다.
2. 우측 상단 커널 선택 → `Polyglot Notebooks` 또는 `C#` 선택
3. 셀을 실행합니다.

---

## ⚠️ 유의사항

- OpenCV 버전, OS, 내부 코덱에 따라 실행 결과가 다를 수 있습니다.
- Chapter8 ~ Chapter10의 일부 예제는 순차적으로 코드를 완성해 나가는 방식이므로, 중간 예제는 단독 실행 시 동작하지 않을 수 있습니다.
- `tessdata`, `saved_model` 등 외부 데이터 파일은 저장소에 포함되어 있지 않으며, 각 Chapter의 안내를 참고하여 직접 준비해야 합니다.
- OpenCV 3.x와 4.x는 일부 API 반환값이 다릅니다.

```python
# OpenCV 3.3 이하
con_img, contours, hierarchy = cv2.findContours(...)

# OpenCV 4.x
contours, hierarchy = cv2.findContours(...)
```

---

## 📦 주요 의존성

```toml
# OpenCVStudyForPython/pyproject.toml
dependencies = [
    "opencv-python>=4.13.0.92",
    "ipykernel>=7.3.0",
    "jedi>=0.20.0",
    "matplot>=0.1.9",
]
```

---

## 📚 참고 자료

- [OpenCV 공식 문서](https://docs.opencv.org/)
- [OpenCvSharp GitHub](https://github.com/shimat/opencvsharp)
- *C#과 파이썬을 활용한 OpenCV4 프로그래밍* — 위키북스

---

## 📄 License

이 저장소는 [MIT License](LICENSE) 를 따릅니다.  
Copyright © 2026 hundong2
