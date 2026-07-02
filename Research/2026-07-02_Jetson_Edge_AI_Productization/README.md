# Jetson Orin Nano Edge AI Productization Project

이 프로젝트는 한화비전 AI연구소 방향성에 맞춰, "순수 모델 연구"보다 "AI 모델을 실제 카메라 제품과 엣지 디바이스에서 동작시키는 역량"을 쌓기 위한 실행 중심 커리큘럼입니다.

목표는 단순히 YOLO를 실행해보는 것이 아니라, 다음 전체 흐름을 직접 몸에 익히는 것입니다.

```text
카메라 입력 -> 데이터 수집 -> 객체 탐지 모델 실행/학습 -> ONNX export
-> TensorRT 최적화 -> Jetson 실시간 추론 -> FPS/latency 리포트
-> 실패 사례 분석 -> 포트폴리오 README 정리
```

## 최종 산출물

8주 후에는 아래를 GitHub 포트폴리오로 제시할 수 있어야 합니다.

- Jetson Orin Nano + 카메라 실시간 객체 탐지 데모
- CSI/USB/RTSP 카메라 입력 파이프라인
- YOLO 기반 객체 탐지 fine-tuning 또는 최소 custom dataset 실험
- ONNX export 파일
- TensorRT engine 파일
- Python 실시간 추론 코드
- FPS, latency, memory 사용량 리포트
- false positive / false negative 분석 리포트
- 다음 단계: C++/C#/VLM 확장 계획

## 빠른 시작

Jetson Orin Nano에서 실행합니다.

```bash
cd ~/RoboticsStudy/Research/2026-07-02_Jetson_Edge_AI_Productization

python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
pip install -r requirements-jetson.txt

python scripts/00_check_jetson.py
python scripts/01_camera_preview.py --source csi
python scripts/02_yolo_camera_infer.py --source csi --model yolo11n.pt
```

USB 카메라라면:

```bash
python scripts/01_camera_preview.py --source usb --device 0
python scripts/02_yolo_camera_infer.py --source usb --device 0 --model yolo11n.pt
```

## 문서 순서

1. [환경 구축 가이드](./environment_guide.md)
2. [8주 실행 커리큘럼](./curriculum.md)
3. [핵심 기술 지식](./concepts.md)
4. [포트폴리오 체크리스트](./portfolio_checklist.md)
5. [면접 꼬리 질문 지식 베이스](./docs/interview_knowledge_base.md)
6. [면접 드릴 카드](./docs/interview_drill_cards.md)
7. [면접 답변 템플릿](./docs/interview_answer_templates.md)

## 스크립트 구성

| 파일 | 목적 |
| --- | --- |
| `scripts/00_check_jetson.py` | Jetson, CUDA, TensorRT, 카메라 환경 점검 |
| `scripts/01_camera_preview.py` | CSI/USB/RTSP 카메라 입력 확인 |
| `scripts/02_yolo_camera_infer.py` | YOLO 실시간 추론, FPS 표시 |
| `scripts/03_export_yolo_onnx.py` | Ultralytics YOLO 모델을 ONNX로 변환 |
| `scripts/04_benchmark_onnxruntime.py` | ONNX Runtime latency 측정 |
| `scripts/05_capture_dataset.py` | 카메라 프레임을 학습 데이터 후보로 저장 |
| `scripts/06_make_run_report.py` | 실행 결과를 Markdown 리포트로 생성 |

## 권장 학습 방향

현재 경력의 강점은 C/C++/C#/Python, 임베디드, 자동화, 로그 분석, 제품화 경험입니다. 따라서 학습 우선순위는 다음이 가장 효율적입니다.

1. Computer Vision 객체 탐지 모델을 직접 실행하고 평가한다.
2. PyTorch/Ultralytics 모델을 ONNX와 TensorRT로 옮긴다.
3. Jetson 카메라 입력부터 실시간 추론까지 연결한다.
4. FPS, latency, memory, false alarm을 수치화한다.
5. 이후 VLM/LLM fine-tuning과 이벤트 설명 기능으로 확장한다.

면접에서 강하게 말할 수 있는 핵심 문장은 이것입니다.

> 저는 모델 정확도만 보지 않고, 카메라 입력, 전처리, 추론 런타임, 후처리, FPS, latency, memory, 실패 사례 분석까지 묶어서 실제 제품 적용 가능성을 검증하는 방향으로 학습했습니다.
