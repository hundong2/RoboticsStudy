# 10. 논문 읽기, 연구 설계, 학술 활동

목표는 단순 구현을 넘어 연구자로서 질문을 만들고, 실험으로 답하고, 논문 형식으로 전달하는 능력을 기르는 것입니다.

## 강의 목표

- CVPR, ICCV, ECCV, NeurIPS, CoRL, ICRA, IROS 논문의 구조를 읽는다.
- related work matrix를 작성한다.
- 연구 질문, hypothesis, baseline, ablation, metric을 설계한다.
- 실패 사례를 숨기지 않고 분석한다.

## 논문 읽기 템플릿

| 항목 | 질문 |
|---|---|
| Problem | 어떤 문제를 푸는가? |
| Assumption | 어떤 조건을 가정하는가? |
| Method | 핵심 방법은 무엇인가? |
| Data | 어떤 데이터와 환경을 쓰는가? |
| Metric | 무엇을 성공으로 보는가? |
| Baseline | 누구와 비교하는가? |
| Ablation | 어떤 구성요소가 중요한가? |
| Failure | 언제 실패하는가? |
| Reproduce | 내가 무엇을 재현할 수 있는가? |

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | 논문 1편 abstract 분석 | 5문장 요약 |
| 2 | introduction claim 분해 | claim table |
| 3 | method diagram 그리기 | architecture sketch |
| 4 | experiment table 읽기 | metric explanation |
| 5 | ablation 찾기 | component table |
| 6 | failure section 찾기 | limitation note |
| 7 | related work matrix | 10편 비교 |
| 8 | 내 연구 질문 작성 | 3개 hypothesis |
| 9 | 실험 계획 작성 | experiment protocol |
| 10 | 2-page proposal | mini paper |

## 실습 과제

1. Diffusion Policy, ViNT/NoMaD, RT-1/RT-2 중 3편을 골라 matrix를 작성한다.
2. 새로운 visual navigation policy 아이디어 1개를 제안한다.
3. baseline 3개, metric 5개, ablation 3개를 설계한다.
4. 예상 실패 사례와 윤리/안전 리스크를 포함한다.

## 통과 기준

- "좋아 보인다"가 아니라 metric과 baseline으로 주장한다.
- 논문 구현과 연구 기여를 구분한다.
- 발표 슬라이드에서 problem-method-result-limitation 흐름을 유지한다.
