# 고정 크기 행렬 커널과 Condition Supervision

## 결정론적 구조의 최소 조건

작은 로봇 제어 커널에서 입력 차원이 고정이면 다음을 명시할 수 있다.

- 행렬 크기와 반복 횟수 고정
- callback 내부 container growth 금지
- message serialization/reporting decimation
- 역행렬/분해 실패의 유한한 fallback
- NaN/Inf, determinant/condition, torque bound의 독립 관측

`std::array`를 썼다는 사실만으로 hard RT가 되지는 않는다. DDS publish, allocator, logging, executor, page fault, kernel scheduling까지 포함한 end-to-end deadline 분석이 별도로 필요하다.

## determinant의 장점과 한계

작은 square matrix determinant는 계산이 싸고 singular 여부를 빠르게 볼 수 있다. 하지만 스케일에 민감하고 어느 방향이 나빠졌는지 알려주지 않는다. 예를 들어 singular value들이 `(1000, 0.001)`이면 determinant는 1이어도 condition number는 매우 나쁘다.

실무 우선순위는 보통 다음과 같다.

1. 입력/출력 물리 단위를 scaling한다.
2. Cholesky/LDLT/SVD 같은 적절한 solve를 사용한다.
3. reciprocal condition estimate 또는 최소 singular value를 기록한다.
4. threshold에 hysteresis를 둔다.
5. degraded mode에서 task gain/차원을 낮추고 torque-rate를 제한한다.

## 감사 지표

- `solve_ok`: factorization/solve 성공 여부
- `condition_metric`: determinant만이 아니라 `rcond` 또는 `sigma_min`
- `regularization`: damping/diagonal loading 값
- `residual`: `||Ax-b||`
- `saturation_count`: actuator/clamp 활성 수
- `execution_us`와 `wakeup_lateness_us`: 계산시간과 scheduling 지연을 분리
- `fallback_count`: degraded command가 실제로 사용된 횟수

## 안전한 주장

“고정 크기이고 평균 20 us였다”는 구조 및 관측 결과다. “500 Hz deadline을 항상 만족한다”는 WCET/스케줄링/부하/하드웨어 조건까지 검증해야 하는 별도 주장이다.
