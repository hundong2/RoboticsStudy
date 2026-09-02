# A* 경로 탐색

## 핵심 식

```text
f(n) = g(n) + h(n)
```

- `g(n)`: 시작에서 현재 노드까지 확정된 비용
- `h(n)`: 현재 노드에서 목표까지 남은 비용의 추정
- open set: 앞으로 확장할 후보
- closed set: 확장을 마친 후보
- parent: 최종 경로를 역추적할 이전 노드

## heuristic 선택

| 이동 모델 | 대표 heuristic |
|---|---|
| 4방향, 단위 비용 | Manhattan `|dx|+|dy|` |
| 8방향, 대각 비용 `sqrt(2)` | Octile |
| 연속 평면의 직선 이동 | Euclidean `sqrt(dx²+dy²)` |

admissible heuristic은 실제 남은 최소 비용을 넘지 않는다. consistent heuristic은 모든 간선에서 `h(n) ≤ c(n,n')+h(n')`를 만족한다. 이동 모델과 cost 정의가 바뀌면 heuristic의 증명도 다시 확인해야 한다.

## 로보틱스에서 추가로 필요한 것

격자 최단 경로가 곧 로봇이 실행할 trajectory는 아니다.

1. footprint와 안전 여유를 costmap inflation에 반영한다.
2. nonholonomic 로봇이면 heading/curvature를 state 또는 motion primitive에 넣는다.
3. path smoothing 후 다시 collision check한다.
4. 속도·가속도·jerk 제한을 반영해 시간 파라미터화한다.
5. 동적 장애물은 local planner/controller 또는 시공간 planner가 담당한다.

## 구현 선택과 RT

- binary heap priority queue: 보통 큰 지도에서 효율적이지만 내부 capacity와 allocation을 통제해야 한다.
- 고정 배열 선형 스캔: 작은 유한 지도에서는 느려도 메모리·반복 상한을 읽기 쉽다.
- preallocated heap: 성능과 결정론의 절충안이다.
- 어떤 구현도 입력 지도 크기, 최대 확장 수, 취소/deadline 정책이 없으면 실시간 상한을 말하기 어렵다.

## 참고

- [원 논문 DOI](https://doi.org/10.1109/TSSC.1968.300136)
- [A* 원 논문 공개 PDF](https://people.stfx.ca/jdelamer/courses/csci-564/_downloads/b2220c66675ddde471ca1795147b8e86/A_Formal_Basis_for_the_Heuristic_Determination_of_Minimum_Cost_Paths.pdf)
- [1972년 저자 정정](https://cse.sc.edu/~MGV/csce580f11/astarHNR1972.pdf)
