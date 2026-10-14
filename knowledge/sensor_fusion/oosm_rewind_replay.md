# Out-of-Sequence Measurement 되감기/재전파

센서 측정 `z_j`가 상태 `x_j`를 관측했지만 filter가 이미 `x_k (k>j)`까지 진행한 뒤 도착하면 OOSM이다. 가장 이해하기 쉬운 해법은 과거 state/covariance와 입력을 bounded window에 저장하고 다음 순서로 처리하는 것이다.

1. `Header.stamp`로 측정 시점 `j`를 찾는다.
2. `x_j, P_j`에 measurement update를 수행한다.
3. 저장된 입력과 실제 stamp 간격으로 `j+1 ... k`를 다시 propagate/update한다.
4. 재전파한 `x_k, P_k`를 현재 상태로 채택한다.

현재 상태에 과거 측정을 바로 적용하면 innovation에 지연 동안의 실제 운동이 섞인다. 일정 속도 `v`, 지연 `delta`라면 위치 residual에는 대략 `v*delta`의 가짜 오차가 추가된다.

제품 계약에는 최대 window age, 최대 state 수, 너무 오래된 측정의 drop 정책, timestamp 역행/clock jump 정책, 중복 측정 정책, interpolation 여부를 명시한다. 비선형 filter에서는 과거 보정 뒤 Jacobian과 preintegration을 다시 계산해야 할 수 있다. 지연이 알려지지 않았다면 time offset을 calibration/state로 추정해야 하며, 충분한 운동 excitation과 consistency 검증이 필요하다.

참고: [Bar-Shalom et al., 2004](https://doi.org/10.1109/TAES.2004.1292140), [Delama et al., 2026](https://arxiv.org/abs/2605.13266).
