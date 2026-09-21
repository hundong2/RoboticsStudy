# Interacting Multiple Model (IMM) 필터

## 언제 쓰는가

IMM은 하나의 연속 동역학만으로 설명하기 어려운 switching system에 적합하다. 정상/고장, 직진/회전, 지상/공중처럼 유한 개 모드가 있고 각 모드에 Kalman 계열 필터를 정의할 수 있을 때 사용한다.

## 한 스텝

모드 전이행렬을 `p_ij`, 이전 모드 확률을 `mu_i`라 하자.

```text
c_j       = sum_i p_ij mu_i
mu_{i|j}  = p_ij mu_i / c_j
x0_j      = sum_i mu_{i|j} x_i
P0_j      = sum_i mu_{i|j}[P_i+(x_i-x0_j)(x_i-x0_j)^T]
```

각 모델 `j`는 `(x0_j,P0_j)`에서 독립 predict/correct하고 innovation likelihood `Lambda_j`를 계산한다.

```text
mu_j(k) = Lambda_j c_j / sum_l Lambda_l c_l
```

필요하면 최종 연속 상태도 `x=sum_j mu_j x_j`로 합친다. fault detection에서는 종종 모드 확률 자체가 더 중요한 출력이다.

## 튜닝과 함정

- `Q`가 너무 작으면 모델 오차를 고장으로 오인한다.
- `R`가 너무 작으면 센서 spike가 모드 확률을 뒤집는다.
- 전이확률의 self-transition이 너무 크면 검출/복구가 느려진다.
- 모델 집합에 실제 원인이 없으면 IMM은 가장 덜 틀린 모델을 고를 뿐이다.
- 모델 조합 수가 폭발하면 state augmentation, generalized pseudo-Bayesian, hierarchical model set을 고려한다.

모드 확률 하나로 바로 장치를 차단하지 말고 지속 시간, hysteresis, 통신 건강성, 독립 안전 채널과 결합한다.

## 참고

- https://doi.org/10.1016/j.ifacol.2018.09.708
