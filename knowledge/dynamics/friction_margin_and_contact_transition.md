# 마찰 여유와 접촉 전이

## Coulomb friction cone

평면 접촉의 가장 단순한 조건은 다음과 같다.

```text
|F_t| <= mu F_n,  F_n >= 0
```

추정 마찰계수의 평균을 그대로 쓰면 과신하기 쉽다. 보수 하한 `mu_lower=mu_mean-k sigma_mu`를 사용하고 절대 여유 `b`를 빼면 명령 한계는 `max(0, mu_lower F_n-b)`가 된다.

## 전이는 한 표본으로 결정하지 않는다

- trip debounce: 위험이 N회 연속일 때 fallback
- minimum hold: fallback 직후 일정 시간은 복귀 금지
- recovery hysteresis: 정상 M회 연속일 때만 복귀
- stale data: sample age가 deadline을 넘으면 위험으로 처리

N/M/hold는 센서 주기와 로봇의 멈춤 시간으로 환산해야 한다. "3표본"만 적으면 sampling rate 변경 시 의미가 달라진다.

## 한계

Coulomb 모델은 stick-slip, Stribeck 효과, compliance, 온도, 표면 오염을 단순화한다. 따라서 friction margin 하나로 안전을 증명하지 말고 torque/rate/energy limit와 HIL fault injection을 결합한다.
