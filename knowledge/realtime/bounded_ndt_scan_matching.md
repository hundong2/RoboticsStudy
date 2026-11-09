# Bounded NDT scan matching

NDT를 실시간 경로에 넣을 때 “평균적으로 빠르다”와 “work upper bound가 있다”를 구분한다. bounded kernel은 최소한 point 수, cell 수, optimizer iteration, linear system 차원, correspondence/gating 탐색의 상한을 고정한다.

## 권장 경계

1. DDS의 가변 `PointCloud2`를 schema/frame/size 검사한다.
2. voxel/downsample 단계에서 최대 point 수의 고정 저장소로 복사한다.
3. 고정 또는 사전 할당된 cell/voxel table을 사용한다.
4. iteration cap, step cap, outlier gate와 solver failure 조건을 둔다.
5. 결과와 함께 used points/cells, score, condition metric, iterations, observed runtime을 보낸다.
6. 별도 supervisor가 stale stamp, pose jump, low information, deadline miss 때 결과를 거부한다.

이 경계만으로 hard RT가 되지는 않는다. DDS/RMW 할당, executor wake-up, page fault, logging, cache/NUMA, OS scheduling을 포함한 end-to-end WCET와 target hardware trace가 별도로 필요하다.

## 2026-11-10 학습 구현의 구체적 상한

- reference/current points: 192/96
- 2D grid: 20×20 fixed cells
- optimization: maximum 6 Gauss–Newton iterations
- solve: fixed 3×3 partial-pivot elimination
- Mahalanobis gate: squared distance 25
- per-iteration trust region: 0.08 m / 0.04 rad
- audit join: two fixed 128-slot exact-stamp rings

이 숫자는 교육용 합성 장면의 budget이며 실제 LiDAR 제품 설정의 권장값이 아니다. 실제 배포에서는 sensor density와 CPU budget으로 다시 산정하고 worst-case failure injection을 수행한다.
