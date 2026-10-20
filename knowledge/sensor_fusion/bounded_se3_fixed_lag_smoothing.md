# Bounded SE(3) Fixed-Lag Smoothing 설계 노트

## 목적

여러 센서의 자세 측정을 factor graph로 융합하면서 메모리와 한 update의 계산 범위를 제한하기 위한 대표 설계 원칙을 누적 정리한다. 여기서 SE(3)는 3D rigid pose를 뜻하며, 실제 구현은 quaternion/rotation matrix와 tangent-space perturbation을 구분해야 한다.

## 메시지 경계 체크리스트

- `header.stamp`는 측정시각이며 도착시각으로 덮어쓰지 않는다.
- `frame_id`와 factor residual이 기대하는 좌표계를 명시한다.
- 쿼터니언을 정규화하고 zero/NaN을 거부한다.
- 6×6 covariance의 순서와 단위를 확인한다.
- 같은 raw observation이 frontend와 backend factor에 중복 반영되지 않았는지 확인한다.
- 서로 상관된 두 pose의 covariance 차를 독립 noise처럼 쓰지 않는다.

## Fixed lag의 세 정책

1. **창 안의 지연 factor:** 원래 측정시각의 state에 연결하고 영향받은 구간을 다시 푼다.
2. **창보다 오래된 factor:** 명시적으로 drop하거나, 별도 batch reprocess 경로로 보낸다.
3. **창 이동:** 단순 삭제하지 말고 남은 state에 prior 정보를 넘긴다. Schur complement는 정보를 보존하지만 dense prior를 만들 수 있다.

## Manifold 선형화

pose `T ∈ SE(3)`에 직접 6-vector를 더하는 대신 tangent perturbation을 사용한다.

\[
T \leftarrow T \operatorname{Exp}(\delta \xi), \qquad \delta\xi \in \mathbb{R}^6
\]

between factor의 전형적 residual은 다음과 같다.

\[
r_{ij} = \operatorname{Log}\left(Z_{ij}^{-1} T_i^{-1} T_j\right)
\]

Euler 차분은 작은 각도 교육에는 유용하지만 큰 회전, branch cut, gimbal lock에 취약하다. 제품 구현은 `Pose3` 같은 manifold type과 검증된 analytic/numerical Jacobian을 사용한다.

## 관측해야 할 진단

- active states/factors와 linear-system dimension
- 새 factor가 영향을 준 state 또는 clique 수
- relinearized variables 수
- update latency의 current/max/histogram
- oldest accepted delay와 too-old drop 수
- solver failure, rank/condition, non-finite 수
- NIS/whitened residual과 robust-loss downweight 수

## 정확도와 실시간성의 경계

fixed-size 배열과 fixed iteration은 계산 상한을 설명하기 쉽게 하지만, DDS와 allocator, cache, OS scheduling을 포함한 WCET 증명은 아니다. 반대로 sparse incremental solver는 평균적으로 빠르지만 큰 loop closure에서 작업량이 늘 수 있다. 안전 요구가 있다면 optimizer 결과의 신선도·유효성·fallback을 별도 supervisor가 판단해야 한다.

## 관련 일일 실습

- `daily_robotics/2026-10-20`: 8-pose dense tangent-space 교육 예제와 지연 absolute factor audit
- 확장 방향: GTSAM `ISAM2`, `Pose3`, robust loss, Schur marginalization, affected-clique budget
