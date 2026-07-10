# 2026-07-10 VLM 주간 종합: 증류, 세계 모델, 센서 융합, 공간 한계

> 기준일: 2026년 7월 10일  
> 대표 이름: **Multiplayer Interactive World Models + VLV Auto-Encoder**  
> 목적: 7월 5일~10일 VLM 흐름을 실습 가능한 기술 관점으로 통합

## 오늘의 결론

이번 주 VLM 흐름은 "큰 모델 하나로 모든 것을 해결"에서 멀어지고 있다. 대신 다음 네 가지가 핵심이다.

1. **비용을 줄인다:** VLV Auto-Encoder, Unlimited OCR, Eve
2. **진짜로 보게 한다:** GAVEL, Perceval, HarmVideoBench
3. **공간과 시간을 이해한다:** HiSpatial, Constructive Apraxia, Multiplayer World Models
4. **위험할 때 다르게 행동한다:** DriveMRP, Risk-Aware Driver Monitoring

## 왜 이걸 만들어야 하나

실무 VLM은 단순 QA demo가 아니다. 로봇, 문서 자동화, 자율주행, 게임 에이전트, 영상 안전성 시스템에서는 다음 요구가 동시에 나온다.

- 긴 문서와 긴 출력을 저렴하게 처리해야 한다.
- 장면 설명이 아니라 위험과 원인을 판단해야 한다.
- 2D pixel에서 3D 관계와 시간 변화를 추론해야 한다.
- 모델이 확신 없을 때 더 비싼 경로로 보내야 한다.
- 환각이 나면 어느 단어와 어느 영역이 틀렸는지 알아야 한다.

그래서 이 폴더에는 주간 통합 실습 프로젝트를 추가했다.

## Top 5 정리

| 순위 | 논문 | 확인된 메타데이터 | 왜 중요한가 |
|---:|---|---|---|
| 1 | VLV Auto-Encoder | arXiv 2507.07104 | diffusion 지식을 활용해 captioning/VLM 학습 비용 절감 |
| 2 | Multiplayer Interactive World Models | arXiv 2607.05352 | 다중 agent action-conditioned world model |
| 3 | RE-VLM | arXiv 2605.19329 | 저조도/HDR/고속 모션에서 RGB 한계 보완 |
| 4 | HiSpatial | arXiv 2603.25411 | 3D spatial understanding을 계층적 task로 학습 |
| 5 | Constructive Apraxia | arXiv 2410.03551 | 최첨단 VLM의 공간 구성 한계 진단 |

## 실습 프로젝트

폴더: [weekly_vlm_practice_project](./weekly_vlm_practice_project)

실행:

```powershell
python Research\2026-07-10_VLM_Weekly_Synthesis_Practice\weekly_vlm_practice_project\weekly_vlm_practice.py
```

실습 항목:

- R-SWA cache 절감 시뮬레이션
- Self-evolving questioner 루프
- GAVEL-style caption error localization
- Risk-aware selective inference gate
- BEV high-risk motion scoring
- Simple multi-agent world rollout

## 실습으로 배울 핵심

### 1. R-SWA

왜 필요한가: 긴 OCR 출력에서는 `KV cache`가 생성 토큰 수에 따라 커진다. 서비스에서는 정확도만큼 메모리 상한이 중요하다.

### 2. Self-evolving Questioner

왜 필요한가: 사람이 만든 질문만으로는 모델의 약점을 계속 찌르기 어렵다. 모델이 스스로 어려운 질문을 생성하고 필터링하면 학습 curriculum이 생긴다.

### 3. GAVEL-style Localization

왜 필요한가: hallucination을 고치려면 "틀렸다"가 아니라 "어떤 claim이 어느 영역 증거와 어긋났는지"를 찾아야 한다.

### 4. Selective Gate

왜 필요한가: 항상 큰 모델을 쓰면 비싸고, 항상 작은 모델을 쓰면 위험하다. fast path와 slow/abstain path를 나누면 비용과 안전을 같이 관리할 수 있다.

### 5. World Model

왜 필요한가: 에이전트는 현재 장면 설명보다 행동 후 미래 장면 예측이 중요하다. 특히 다중 agent 환경에서는 각 agent 행동의 attribution이 필요하다.

## 검증/정정 메모

- VLV Auto-Encoder는 2026년 신규가 아니라 2025년 arXiv 논문이다.
- Eve도 2026년 7월 신규가 아니라 2025년 arXiv/AAAI 계열 논문이다.
- GAVEL 번호는 원문 `2606.17643`이 아니라 `2606.26923`으로 확인된다.
- HarmVideoBench 번호는 원문 `2606.17700`이 아니라 `2606.27187`로 확인된다.
- Risk-Aware Driver Monitoring 번호는 원문 `2606.17671`이 아니라 `2606.26922`로 확인된다.

## 참고 링크

- VLV Auto-Encoder: https://arxiv.org/abs/2507.07104
- Multiplayer Interactive World Models: https://arxiv.org/abs/2607.05352
- RE-VLM: https://arxiv.org/abs/2605.19329
- HiSpatial: https://arxiv.org/abs/2603.25411
- Constructive Apraxia: https://arxiv.org/abs/2410.03551
- HarmVideoBench: https://arxiv.org/abs/2606.27187
- GAVEL: https://arxiv.org/abs/2606.26923
- Risk-Aware Driver Monitoring: https://arxiv.org/abs/2606.26922
