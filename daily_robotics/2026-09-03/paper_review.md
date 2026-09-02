# 논문 리뷰 — A Formal Basis for the Heuristic Determination of Minimum Cost Paths

## 서지 정보

- 저자: Peter E. Hart, Nils J. Nilsson, Bertram Raphael
- 학술지: *IEEE Transactions on Systems Science and Cybernetics*, 4(2), 100–107
- 발표: 1968년
- DOI: [10.1109/TSSC.1968.300136](https://doi.org/10.1109/TSSC.1968.300136)
- 공개본: [논문 PDF](https://people.stfx.ca/jdelamer/courses/csci-564/_downloads/b2220c66675ddde471ca1795147b8e86/A_Formal_Basis_for_the_Heuristic_Determination_of_Minimum_Cost_Paths.pdf)
- 후속 정정: [Correction to “A Formal Basis...” (1972)](https://cse.sc.edu/~MGV/csce580f11/astarHNR1972.pdf)

이 논문은 최신 논문 대신 오늘의 알고리즘을 세운 **역사적 근간 논문**으로 선정했다. 현대의 Nav2 global planner, 로봇 팔의 configuration-space 탐색, 게임 경로 탐색까지 이어지는 “휴리스틱으로 탐색량을 줄이면서 언제 최단 경로를 지킬 수 있는가”의 이론적 출발점이다.

## 1. 해결하려는 문제

그래프에서 최소 비용 경로를 찾는 문제 자체는 논문 이전에도 알려져 있었다. 문제는 도메인 지식으로 “목표가 대략 어느 쪽인가”를 알면서도, 이를 탐색 순서에 넣는 공통 이론이 없었다는 점이다.

- Dijkstra류 탐색은 이미 지불한 비용만 보므로 안전하지만 목표와 반대 방향까지 넓게 탐색한다.
- 목표까지의 거리 추정을 너무 공격적으로 믿으면 빨리 도착할 수는 있어도 최단 경로를 놓친다.
- 당시의 여러 탐색 전략을 같은 언어로 비교하고, 어떤 조건에서 정답이 보장되는지 설명할 틀이 필요했다.

로봇으로 바꾸어 말하면, 10만 개 격자 셀을 모두 확인하지 않고 goal 방향의 셀을 우선 보고 싶지만, 벽 뒤의 지름길을 성급히 포기해서는 안 된다는 문제다.

## 2. 핵심 아이디어와 수학적 직관

논문의 핵심은 각 후보 노드 `n`을 다음 평가 함수로 정렬하는 것이다.

```text
f(n) = g(n) + h(n)
```

- `g(n)`: 시작부터 `n`까지 실제로 확인된 누적 비용
- `h(n)`: `n`부터 목표까지 남은 비용의 추정치
- `f(n)`: 이 노드를 거치는 전체 경로 비용의 예상 하한

### 왜 admissible heuristic이 중요한가

`h(n) ≤ h*(n)`이면 heuristic이 실제 남은 최적 비용 `h*`를 과대평가하지 않는다. 이런 `h`를 admissible하다고 한다. 목표로 가는 유망한 경로를 실제보다 나쁘다고 잘못 평가하지 않으므로, 적절한 A*는 최적 목표보다 비싼 목표를 먼저 정답으로 확정하지 않는다.

오늘의 4방향 격자에서는 한 번 움직일 때 x 또는 y가 1만 바뀐다. 따라서

```text
h(n) = |x_goal - x_n| + |y_goal - y_n|
```

인 Manhattan 거리는 장애물이 없을 때 필요한 최소 이동 수다. 장애물은 우회를 늘릴 뿐 이 값보다 실제 경로를 짧게 만들지 못하므로 admissible하다.

### consistency가 구현에 주는 이점

간선 비용을 `c(n,n')`라 하면 consistent heuristic은 다음 삼각부등식을 만족한다.

```text
h(n) ≤ c(n,n') + h(n')
```

그러면 경로를 따라 `f`가 감소하지 않는다. 한 번 가장 좋은 값으로 닫은 노드를 나중에 더 싼 경로 때문에 다시 열 필요가 없어져 구현과 실행시간 분석이 단순해진다. Manhattan 거리와 단위 4방향 이동은 이 조건도 만족한다.

### 1972년 정정을 함께 읽어야 하는 이유

원 논문의 “어떤 A*도 같은 휴리스틱 정보를 쓰는 다른 admissible 알고리즘보다 더 적은 노드를 확장할 수 없다”는 최적 효율성 논증에는 consistency와 tie-breaking에 관한 표현 문제가 알려졌고 저자들이 정정을 냈다. 실무 교훈은 짧다. “A*가 최적”이라는 말을 다음 세 가지로 분리해야 한다.

1. 반환 경로 비용이 최적인가?
2. 확장 노드 수가 이론적으로 최소인가?
3. 실제 wall-clock latency와 메모리 사용량이 최소인가?

이 셋은 같은 주장이 아니다.

## 3. 실무 적용 가능성과 한계

### 적용하기 좋은 곳

- 정적 또는 천천히 변하는 occupancy/cost grid의 global planning
- 로봇 팔의 이산화된 configuration space 탐색
- task planning처럼 행동 전이 비용과 목표까지의 하한을 만들 수 있는 문제
- Dijkstra 대비 확장량을 줄이되 경로 최적성 증거가 필요한 시스템

ROS 2/Nav2 관점에서는 costmap 셀을 graph node로, 인접 이동을 edge로, 이동 거리와 inflation cost를 edge cost로 볼 수 있다. 오늘 코드의 `g_score_`, `heuristic()`, `parent_`가 바로 이 세 역할을 맡는다.

### 제품 코드에서 드러나는 한계

1. **메모리 규모:** 실제 2D map은 400셀이 아니라 수백만 셀일 수 있다. 모든 셀의 `g`, parent, state를 잡으면 메모리가 커진다.
2. **동적 환경:** 사람이 새로 나타날 때 처음부터 A*를 다시 돌리면 비싸다. D* Lite, LPA* 같은 증분 탐색이나 local planner와의 분리가 필요하다.
3. **로봇 운동학:** 4방향 셀 경로는 자동차형 로봇의 최소 회전반경, 속도, 가속도 제약을 표현하지 못한다. Hybrid A*, state lattice, trajectory optimization이 필요하다.
4. **휴리스틱과 cost 일치:** 대각 이동, 경사, 지형 위험도를 cost에 넣으면서 Manhattan h를 함부로 확대하면 admissibility가 깨질 수 있다.
5. **RT 보장 부재:** A*의 입력 크기가 유한해도 cache miss, TF/DDS, 동적 할당, OS scheduling 때문에 end-to-end deadline이 자동 보장되지는 않는다.
6. **경로 품질:** 격자 경로는 각지고 장애물에 가까울 수 있다. smoothing 뒤 충돌 검사를 다시 하고, controller가 추종 가능한 곡률인지 확인해야 한다.

## 오늘 코드에 적용한 설계 판단

| 논문 개념 | 코드 대응 | 엔지니어링 판단 |
|---|---|---|
| `g(n)` | `g_score_` | 시작에서 셀까지의 단위 누적 비용 |
| `h(n)` | `heuristic()` | 4방향 모델에 맞는 consistent Manhattan 거리 |
| open set | `NodeState::kOpen` | heap 대신 전체 배열 스캔으로 메모리 상한을 명확화 |
| closed set | `NodeState::kClosed` | consistent h를 전제로 재개방하지 않음 |
| 경로 복원 | `parent_`, `reverse_path` | goal에서 start까지 고정 배열에 저장 |
| 탐색 비용 관찰 | `/planning/expanded_nodes` | 휴리스틱 변경 효과를 수치로 비교 |

## 주니어 엔지니어에게 남길 질문

- `h=0`이면 왜 A*가 Dijkstra와 같은 탐색 순서를 보이는가?
- 모든 heuristic에 2를 곱하면 어떤 지도에서 최단 경로를 놓칠 수 있는가?
- priority queue가 더 빠른데도 오늘 선형 스캔을 택한 RT 설계 이유는 무엇인가?
- 경로 최적성과 제어 가능한 trajectory는 왜 다른가?
- costmap이 갱신될 때 전체 재탐색 대신 증분 탐색을 선택할 기준은 무엇인가?
