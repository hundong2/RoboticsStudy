# 2026-07-09 VLM 리서치 노트: 안전성 벤치마크와 선택적 추론

> 기준일: 2026년 7월 9일  
> 대표 이름: **HarmVideoBench / GAVEL / Risk-Aware Monitoring**  
> 핵심 질문: VLM은 위험한 상황에서 언제 확신하지 말아야 하는가?

## 오늘의 결론

7월 9일 내용은 7월 8일과 거의 같은 축을 반복하지만, 더 실무적인 해석은 **선택적 추론(selective inference)**이다. 모델은 모든 입력에 답해야 하는 것이 아니라, 위험도가 높거나 근거가 부족하면 abstain, verify, slow branch를 선택해야 한다.

## 왜 필요한가

안전 도메인에서 "항상 답하기"는 위험하다.

- 유해 영상 판단: 맥락이 부족하면 추가 evidence가 필요하다.
- caption 검증: 시각 증거 위치를 못 찾으면 확정하면 안 된다.
- driver monitoring: 위험 신호가 약하면 경고/검증 branch로 보내야 한다.

## 핵심 논문

| 논문 | 역할 |
|---|---|
| HarmVideoBench | harmful video reasoning의 계층적 평가 |
| GAVEL | image-caption mismatch의 검증/설명/위치 특정 |
| Risk-Aware Driver Monitoring | fast model + selective gate + world model |
| Multiplayer Interactive World Models | 동적 환경의 미래 예측 |
| Unlimited OCR | 긴 입력/출력 비용 최적화 |

## 선택적 추론 구조

```text
fast model prediction
-> confidence / risk / future error estimate
-> accept_fast or abstain_warn or slow_verify
```

이 구조는 driver monitoring뿐 아니라 VLM 서비스 전반에 적용할 수 있다.

## 왜 써야 하나

- 비용이 낮다: 대부분 쉬운 입력은 fast path로 처리한다.
- 안전하다: 위험 입력은 abstain 또는 검증 branch로 보낸다.
- 설명 가능하다: gate가 왜 보류했는지 기록할 수 있다.
- 운영 가능하다: latency budget과 risk budget을 같이 관리한다.

## 실습 연결

7월 10일 통합 실습 프로젝트의 `selective_gate_demo()`가 이 구조를 재현한다.

## 참고 링크

- HarmVideoBench: https://arxiv.org/abs/2606.27187
- GAVEL: https://arxiv.org/abs/2606.26923
- Risk-Aware Driver Monitoring: https://arxiv.org/abs/2606.26922
- Multiplayer Interactive World Models: https://arxiv.org/abs/2607.05352
- Unlimited OCR Works: https://arxiv.org/abs/2606.23050
