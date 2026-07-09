# 논문 작성 블루프린트

## 제목

Confidence-Gated Multimodal Evidence Fusion for Robust Field Document Question Answering

## 문제 정의

현장형 문서 QA 시스템은 다음 세 가지 실패 요인을 동시에 겪습니다.

- 음성 명령은 노이즈, 겹침 발화, 다국어 숫자 코드에서 오류가 생깁니다.
- OCR은 흐림, 회전, 낮은 대비, 레이아웃 복잡도에서 오류가 생깁니다.
- 환경음은 작업 실행 여부를 바꿔야 하는 안전 신호가 될 수 있습니다.

## 제안 방법

ASR confidence, OCR quality, layout match, environmental sound risk를 분리 추정한 뒤,
confidence-gated fusion으로 QA 후보를 선택하고 위험 환경음이 있으면 실행형 응답을 확인 질문으로 바꿉니다.

## 최소 실험

- Baseline 1: OCR-only text QA
- Baseline 2: OCR + layout hint
- Baseline 3: ASR + OCR
- Proposed: ASR + OCR + layout + sound safety gate

## 주요 지표

- QA accuracy / exact match
- CER/WER by condition
- unsafe action rate
- latency
- condition-wise robustness
- bootstrap confidence interval

## 논문 기여점

1. 음성, OCR, 환경음 신호를 함께 다루는 현장형 문서 QA 평가 프로토콜
2. 해석 가능한 confidence-gated multimodal fusion
3. 정확도와 안전 지표를 함께 보는 ablation 및 오류 분석

## 한계

- 합성 데이터만으로는 실제 현장 분포를 대표할 수 없습니다.
- 대형 end-to-end 모델과 직접 비교하려면 충분한 공개 데이터와 GPU 예산이 필요합니다.
- 회사명, 고객명, 사내 문서명은 모두 `xxx`로 마스킹해야 합니다.
