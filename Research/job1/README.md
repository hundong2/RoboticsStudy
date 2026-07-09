# 멀티모달 음성·비전·언어 연구 커리큘럼

이 커리큘럼은 음성 인식, 음성 대화, 환경음 이해, OCR/문서 이해, VLM, 데이터 전처리, 실험 관리, 기술 리드, 논문 작성 역량을 한 번의 연구 프로젝트로 연결합니다.

회사명, 고객명, 소속명처럼 민감하거나 특정 회사를 가리키는 정보는 본문과 산출물에서 `xxx`로 표기합니다.

## 최종 논문 목표

**Confidence-Gated Multimodal Evidence Fusion for Robust Field Document Question Answering**

핵심 질문은 다음과 같습니다.

- ASR+OCR 결합은 OCR-only 문서 QA보다 노이즈 조건에서 정확도를 높이는가?
- 환경음 safety gate는 QA 정확도를 크게 희생하지 않고 unsafe action rate를 낮추는가?
- OCR 품질 점수와 ASR 오류 유형을 사용한 confidence fusion은 단순 결합보다 강건한가?

## 실행 순서

1. `00_orientation/00_job_to_research_roadmap.ipynb`
2. `01_audio_speech_language/01_audio_feature_baselines.ipynb`
3. `01_audio_speech_language/02_asr_evaluation_error_analysis.ipynb`
4. `01_audio_speech_language/03_environmental_sound_understanding.ipynb`
5. `02_vision_ocr_language/01_ocr_robustness_lab.ipynb`
6. `02_vision_ocr_language/02_document_layout_docvqa_baseline.ipynb`
7. `02_vision_ocr_language/03_image_text_retrieval_vlm_baseline.ipynb`
8. `03_multimodal_dialogue/01_speech_dialogue_state_machine.ipynb`
9. `04_data_mlops_leadership/01_data_engineering_quality.ipynb`
10. `04_data_mlops_leadership/02_experiment_tracking_and_model_cards.ipynb`
11. `04_data_mlops_leadership/03_code_review_mentoring_playbook.ipynb`
12. `05_research_paper_project/01_literature_review_matrix.ipynb`
13. `05_research_paper_project/02_reproduction_and_ablation.ipynb`
14. `05_research_paper_project/03_final_multimodal_paper_prototype.ipynb`
15. `05_research_paper_project/04_paper_figures_and_draft.ipynb`

## 폴더 구조

```text
00_orientation/                  직무 요구사항을 역량 지도와 연구 주제로 변환
01_audio_speech_language/         음성 특징량, ASR 평가, 환경음 이해
02_vision_ocr_language/           OCR 강건성, 문서 레이아웃 QA, 이미지-텍스트 검색
03_multimodal_dialogue/           음성 대화 상태와 안전 확인 정책
04_data_mlops_leadership/         데이터 품질, 실험 관리, 모델 카드, 코드 리뷰
05_research_paper_project/        문헌조사, 재현/ablation, 최종 논문 프로토타입
data/                             공개 데이터셋 연결 메모
templates/                        문헌조사/실험로그/리뷰 체크리스트 템플릿
scripts/                          노트북 생성 및 검증 스크립트
```

## 환경 준비

최소 실습:

```powershell
python -m pip install -r requirements-minimal.txt
```

선택 실습:

```powershell
python -m pip install -r requirements-optional.txt
```

대형 모델이나 공개 데이터셋을 내려받는 셀은 기본 실행 경로에서 제외했습니다. 먼저 합성 데이터 실습으로 평가 틀을 만들고, 이후 공개 데이터셋으로 교체하세요.

## 완료 산출물

- `artifacts/skill_to_notebook_map.csv`
- `artifacts/asr_error_summary.csv`
- `artifacts/ocr_robustness_eval.csv`
- `artifacts/docqa_mini_eval.csv`
- `artifacts/data_card.json`
- `artifacts/experiment_log.csv`
- `artifacts/literature_review_matrix.csv`
- `artifacts/ablation_bootstrap_ci.csv`
- `artifacts/final_prototype_metrics.csv`
- `artifacts/paper_outline.json`
- `artifacts/paper_table_template.tex`

이 산출물을 모으면 4~6쪽 워크숍 논문 또는 기술 리포트 초안을 작성할 수 있습니다.
