# 데일리 실습 운영표

이 문서는 매일 무엇을 해야 하는지 정리한 실행표입니다. 하루 단위 실습은 작게 유지하고, 매주 하나의 산출물을 남깁니다.

## 매일 고정 루틴

| 시간 | 활동 | 체크 |
|---:|---|---|
| 10분 | 오늘 목표 1개 작성 | 목표가 측정 가능한가? |
| 30분 | 개념 복습 | 전날 막힌 점 해결 |
| 60분 | 구현 | 코드 실행 결과 있음 |
| 40분 | 실험 | metric 또는 plot 있음 |
| 30분 | 논문/문서 읽기 | 5문장 요약 |
| 20분 | 실패 분석 | 다음 실험 1개 정의 |

## 주간 산출물

| 요일 | 산출물 |
|---|---|
| 월 | 개념 요약 1쪽 |
| 화 | 최소 동작 코드 |
| 수 | 실험 1개 |
| 목 | ablation 또는 비교 |
| 금 | 실패 분석 |
| 토 | 논문 1편 리뷰 |
| 일 | 주간 리포트와 다음 주 계획 |

## 12주 압축 루트

시간이 부족하면 24주 과정을 12주로 압축할 수 있습니다.

| 주차 | 목표 | 반드시 남길 것 |
|---:|---|---|
| 1 | Python/PyTorch/로봇 데이터 기초 | tensor/dataset 실습 |
| 2 | 학습 파이프라인 | checkpoint/log |
| 3 | BC baseline | loss curve |
| 4 | sequence/action chunking | ablation |
| 5 | RL/Offline RL 개념 | 비교표 |
| 6 | VAE/diffusion 기초 | toy sampler |
| 7 | diffusion/flow matching action | action sample plot |
| 8 | visual navigation/memory | topological graph |
| 9 | VLA/world model | schema/rollout note |
| 10 | simulator closed-loop | eval protocol |
| 11 | on-device optimization | latency table |
| 12 | capstone report | demo/report |

## 매일 질문 5개

1. 오늘 다룬 observation과 action의 shape은 무엇인가?
2. 이 metric은 실제 로봇 성공과 얼마나 연결되는가?
3. 실패한 sample은 어떤 distribution shift를 보여주는가?
4. 같은 실험을 내일 재현할 수 있는가?
5. 이 결과를 논문 표나 그림으로 바꾸면 무엇이 되는가?
