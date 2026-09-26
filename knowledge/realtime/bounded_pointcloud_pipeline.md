# 고정용량 PointCloud 처리 파이프라인

## 상한을 먼저 정한다

센서 callback의 실행시간은 입력 크기에 비례한다. `max_points`, `max_imu_samples`, `max_iterations`를 설정하고 초과 시 truncate가 안전한지 reject가 안전한지 시스템 위험 분석으로 결정한다. SLAM 품질에서는 조용한 truncate보다 명시적 degradation 상태가 더 낫다.

## 메모리 경계

- hot path는 `std::array`/고정 ring을 사용한다.
- message parsing과 numerical kernel을 분리한다.
- publish 직렬화/복사는 RMW와 message loan 지원 여부를 측정한다.
- ring overwrite, stale sample, missing bracket, malformed field를 각각 계수한다.

## 실행 경계

SingleThreadedExecutor는 공유 배열 접근을 단순하게 하지만 긴 cloud callback이 IMU 수신을 지연시킬 수 있다. 생산 설계에서는 짧은 subscription callback이 bounded SPSC queue에 복사하고, 우선순위가 분리된 worker가 처리하도록 구성할 수 있다. 이때 queue가 가득 찼을 때의 drop-oldest/drop-newest 정책과 sequence gap 진단이 필수다.

## 측정의 올바른 해석

평균/최대 callback 표본은 회귀 탐지에 유용하지만 WCET가 아니다. page fault를 제거하고, CPU affinity/주파수/IRQ/RMW를 고정하고, 충분한 tail sample과 `ros2_tracing` 자료를 확보한 뒤 deadline miss 정책까지 시험해야 한다.

