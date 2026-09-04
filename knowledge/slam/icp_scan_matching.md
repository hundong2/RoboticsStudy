# ICP Scan Matching

## 문제 정의

두 점군의 대응을 모르는 상태에서 강체 변환 \((R,t)\)를 찾아 정합한다.

\[
(R^*,t^*)=\arg\min_{R,t}\sum_i\|p_i-(Rq_i+t)\|^2
\]

여기서 \(q_i\)는 움직일 source/current 점, \(p_i\)는 target/previous 점이다. 코드와 문서에서 변환 방향을 반드시 명시해야 pose 부호가 뒤집히는 버그를 피할 수 있다.

## 기본 반복

1. 초기 변환을 source에 적용한다.
2. target에서 최근접점을 찾아 대응쌍을 만든다.
3. 대응을 고정하고 최소제곱 강체 변환을 계산한다.
4. 오차/변환 변화량/반복 상한으로 종료한다.

2D point-to-point의 닫힌형 회전은 중심을 뺀 대응점 \(q_i,p_i\)에 대해 다음과 같다.

\[
\theta=\operatorname{atan2}\left(\sum_i(q_{ix}p_{iy}-q_{iy}p_{ix}),
\sum_i(q_{ix}p_{ix}+q_{iy}p_{iy})\right),\quad
t=\bar p-R(\theta)\bar q
\]

## 수렴을 해석하는 법

ICP의 오차가 감소해도 올바른 전역 정합이라는 보장은 없다. 최근접점 대응은 초기값과 장면 기하에 의존하므로 지역 최솟값, 대칭 오정합, 좁은 basin 문제가 남는다.

## 제품화 체크리스트

- wheel odometry/IMU/coarse feature로 초기값을 제공한다.
- voxel/downsample로 입력 수와 시간을 제한한다.
- 최대 대응 거리, 법선 각도, trimmed ratio, robust loss로 outlier를 줄인다.
- 점-점보다 점-평면/점-선 오차가 장면에 맞는지 비교한다.
- kd-tree 구축 비용과 query 비용, 메모리 할당을 함께 측정한다.
- 대응 수, RMSE, inlier ratio, Hessian condition으로 퇴화를 감지한다.
- scan-to-scan 누적 drift는 scan-to-map, pose graph, loop closure로 보정한다.

## RT 관점

고정 반복은 시작일 뿐이다. 입력 점 수, 대응 탐색 상한, 자료구조 갱신, 메모리 할당, SIMD/캐시, 최악 장면을 포함해 WCET를 측정해야 한다. 평균 RMSE와 평균 실행 시간만으로 안전 제어 deadline을 주장할 수 없다.

## 관련 실습과 근간 논문

- `daily_robotics/2026-09-05`: 최대 720점, 8회 반복, 0.35 m gate의 2D point-to-point ICP
- [Besl & McKay, 1992, A Method for Registration of 3-D Shapes](https://doi.org/10.1109/34.121791)
