# 계산 상한이 있는 Factor-Graph 수치 커널

## 먼저 상한을 설계한다

Online estimator의 계산량은 sensor rate가 아니라 다음 상한으로 설명해야 한다.

- 활성 변수 수 `N_max`
- factor 수/각 factor 차수
- nonlinear/IRLS 반복 수 `K_max`
- 선형 solver 반복 또는 직접분해 차원
- 한 callback에서 association할 최대 측정 수
- marginalization 빈도와 생성되는 prior 크기

예를 들어 dense Gaussian elimination은 `O(N³)`이지만 `N≤16`이 고정이면 실행 연산 수도 유한하다. 반대로 sparse solver는 평균적으로 훨씬 빠르더라도 큰 loop closure에서 affected clique가 커질 수 있다.

## Hot path와 ROS 경계를 분리한다

1. Subscription callback은 message validation과 고정 크기 measurement copy만 한다.
2. Numerical kernel은 fixed-capacity storage와 명시적 반복 상한을 쓴다.
3. ROS message serialization, logging, diagnostics는 decimation하거나 별도 non-RT thread로 넘긴다.
4. 마지막 정상 추정의 age/deadline이 초과되면 controller가 사용할 fallback을 별도로 둔다.

`std::array`를 썼다는 사실만으로 `rclcpp::publish`, DDS, allocator, page fault, kernel scheduling까지 allocation-free가 되는 것은 아니다.

## 반드시 내보낼 수치 증거

- callback/kernel execution histogram과 표본 최대값
- active variables/factors/iterations
- rejected/late/dropped measurement 수
- pivot, residual, condition proxy, damping 사용 여부
- marginalization count와 prior 크기
- innovation/outlier weight와 covariance sanity
- estimator output age와 deadline miss 수

표본 최대값은 regression과 현장 관측에 유용하지만 WCET 증명과 구분해서 표현한다.

## 수치 fallback

- non-finite 입력은 factor 생성 전에 거부
- pivot/condition 한계 초과 시 새 해 publish 중단 또는 마지막 정상 해 유지
- update step norm 제한으로 폭주 방지
- covariance가 음수/비유한이면 health fault
- graph reset은 원인, 새 datum, frame continuity를 함께 기록

## 제품 검증

- 동일 bag 반복 replay에서 결정적 결과와 latency 분포 비교
- CPU/IO/DDS 부하, page fault, clock jump, delayed measurement 주입
- outlier burst, long bias, sensor dropout, covariance 과신/과소신뢰 시험
- PREEMPT_RT, CPU affinity, `mlockall`, tracing은 numerical bound와 별도로 검증
