# 논문 리뷰 — RRT-Connect: An Efficient Approach to Single-Query Path Planning

## 논문 정보

- 저자: James J. Kuffner Jr., Steven M. LaValle
- 학회: IEEE International Conference on Robotics and Automation (ICRA), 2000, pp. 995–1001
- DOI: [10.1109/ROBOT.2000.844730](https://doi.org/10.1109/ROBOT.2000.844730)
- 원문: [저자 공개 PDF](https://www.clear.rice.edu/comp450/papers/kuffner_lavalle_00.pdf)

이 논문은 “최신 모델”을 쫓기보다 오늘날 OMPL/MoveIt 계열 sampling planner를 이해하는 근간을 고른 것이다. 논문이 실제로 주장한 성질과 현대 제품에서 추가로 필요한 안전·시간 계약을 분리해 읽는다.

## 1. 해결하려는 문제

로봇의 자세를 구성 `q`라고 하자. 충돌하지 않는 구성의 집합 `C_free` 안에서 시작 `q_init`과 목표 `q_goal`을 잇는 연속 경로를 찾아야 한다. 고자유도 로봇에서는 장애물을 기하학적으로 모두 전개한 완전 알고리즘의 계산량이 커지고, 한 번의 동작 요청을 빠르게 푸는 **single-query planning**에는 긴 전처리도 부담이다.

당시의 randomized potential field는 빠를 수 있지만 local minimum 탈출이 불안정했고, probabilistic roadmap은 반복 query에는 좋지만 roadmap 전처리가 필요했다. 저자들이 겨냥한 빈칸은 다음과 같다.

> 전처리 없이, 복잡하고 고차원인 구성 공간을 넓게 탐색하면서도 현재 start–goal 한 쌍을 공격적으로 연결하는 단순한 planner.

## 2. 핵심 아이디어와 수학적 직관

### RRT가 빈 공간 쪽으로 자라는 이유

무작위 표본 `q_rand`를 하나 뽑고 기존 tree에서 가장 가까운 `q_near`를 찾는다. `q_near`에서 표본 방향으로 짧게 전진해 충돌하지 않으면 `q_new`를 추가한다.

```text
q_near = argmin(q ∈ T) ρ(q, q_rand)
q_new  = q_near + δ · unit(q_rand - q_near)
```

tree에서 멀리 떨어진 vertex는 큰 Voronoi 영역을 갖는다. 균일 표본이 그 영역에 떨어질 확률도 크므로 탐색되지 않은 공간의 경계가 자연스럽게 선택된다. 별도의 “미탐색 보상” 함수를 설계하지 않아도 된다.

### `EXTEND`보다 공격적인 `CONNECT`

기본 `EXTEND`는 한 step만 전진하고 `Trapped / Advanced / Reached`를 돌려준다. `CONNECT`는 target에 닿거나 충돌할 때까지 같은 방향으로 `EXTEND`를 반복한다. 넓은 자유 공간에서는 여러 iteration을 아껴 긴 구간을 한 번에 잇는다.

### 두 tree가 서로를 찾는다

한 tree는 `q_init`, 다른 tree는 `q_goal`에서 시작한다.

1. tree A를 무작위 표본으로 한 step 확장한다.
2. tree B를 새 node까지 greedy `CONNECT`한다.
3. 만나지 못하면 A/B 역할을 바꾸고 반복한다.
4. 만나면 각 node의 parent chain을 이어 완전한 경로를 만든다.

이 구조는 RRT의 공간 탐색 편향과 양방향 탐색의 거리 단축을 결합한다. 논문은 충분한 시간이 주어졌을 때 해가 존재하면 성공 확률이 1로 수렴하는 probabilistic completeness와 vertex 분포의 성질을 논의한다. 이는 특정 deadline 안에 반드시 성공한다는 뜻은 아니다.

### 논문 속 실험을 읽는 법

저자들은 7-DOF human arm chess motion에서 평균 2초 미만, 4,500개가 넘는 triangle의 piano 예제에서 100회 평균 12.5초, 좁은 조립 정비 장면에서 평균 17초를 보고했다. 중요한 메시지는 절대 시간이 아니라 다음 두 관찰이다.

- 열린 공간에서는 greedy CONNECT가 큰 이득을 준다.
- 좁은 통로와 collision checking 비용이 지배적인 장면에서는 훨씬 느려진다.

2000년의 CPU/충돌 엔진/장면에서 얻은 평균값을 오늘 로봇의 deadline 근거로 재사용하면 안 된다.

## 3. 실무 적용 가능성과 한계

### 어디에 잘 맞는가

- 로봇 팔의 pick-and-place처럼 start와 goal이 명확한 단발성 joint-space planning
- 높은 자유도 때문에 grid search가 사실상 불가능한 문제
- 최적 경로보다 빠른 feasible path가 먼저 필요한 replanning seed
- bidirectional search가 유리한 비교적 정적인 planning scene

### 제품에 넣기 전에 채워야 할 빈칸

1. **좁은 통로:** 균일 표본이 통로에 들어갈 확률이 작다. bridge test, obstacle-based sampling, projection, constraint manifold sampler가 필요할 수 있다.
2. **경로 품질:** RRT-Connect는 길이·smoothness·jerk 최적화를 목표로 하지 않는다. shortcut/smoothing과 time parameterization 뒤 충돌을 다시 검사해야 한다.
3. **동역학:** 원 논문의 버전은 differential constraint가 없는 구성 공간 문제에 맞춰졌다. torque, velocity, acceleration, nonholonomic constraint는 별도 trajectory generation 또는 kinodynamic planner가 필요하다.
4. **동적 환경:** planning 중 scene이 변하면 성공한 경로도 낡는다. scene version, continuous collision monitoring, execution-time stop layer가 필요하다.
5. **확률과 deadline:** probabilistic completeness는 deadline 보장이 아니다. 반복/node/memory budget과 failure policy를 제품 계약으로 추가해야 한다.
6. **최근접 탐색 비용:** 단순 선형 검색은 node 수에 비례한다. 큰 tree에서는 approximate nearest neighbor가 유리하지만 metric과 index 갱신 비용을 함께 재야 한다.
7. **안전 여유:** collision model의 mesh 오차, joint backlash, state latency를 반영한 inflation과 독립 safety monitor가 필요하다.

## 오늘 코드와 논문의 대응

| 논문 개념 | 오늘 구현 | 의도적 단순화/차이 |
|---|---|---|
| `C`, `C_free` | 2D `(q1,q2)`, 원형 금지 영역 | 실제 link mesh/자기충돌 없음 |
| `RANDOM_CONFIG` | 고정 seed xorshift32 | 재현성 우선, 암호학적 난수 아님 |
| `NEAREST_NEIGHBOR` | 최대 512 node 선형 탐색 | k-d tree 없음 |
| `EXTEND` | 0.14 rad 전진, 0.025 rad collision sample | discrete collision check |
| `CONNECT` | 최대 512회의 bounded greedy extend | 무한 탐색 대신 용량 실패 |
| 두 tree 교대 | start/goal tree swap | 원 논문 흐름 유지 |
| 경로 반환 | `JointTrajectory` + nominal time | smoothing/동역학 최적화 없음 |
| 실행 안전 | 별도 100 Hz collision scaling servo | 원 논문의 planner 밖 제품 계층 |

## 엔지니어가 기억할 한 문장

RRT-Connect의 강점은 “최적성”이 아니라 **큰 구성 공간에서 start와 goal 양쪽을 탐욕적으로 잇는 단순하고 강력한 feasible-path 탐색**이며, 제품의 시간·동역학·충돌 안전은 planner 밖의 명시적 계약으로 완성해야 한다.
