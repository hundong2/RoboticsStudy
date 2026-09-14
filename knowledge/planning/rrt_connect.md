# RRT-Connect 실무 노트

RRT-Connect는 start와 goal에서 두 Rapidly-exploring Random Tree를 키우고, 한 tree의 새 node를 향해 다른 tree가 탐욕적으로 전진하는 single-query sampling planner다.

## 핵심 연산

- `SAMPLE`: 구성 공간에서 후보 자세를 뽑는다.
- `NEAREST`: tree에서 표본과 가장 가까운 node를 찾는다.
- `EXTEND`: 최대 step size만큼 전진하고 edge 충돌을 검사한다.
- `CONNECT`: trapped 또는 reached까지 같은 target으로 EXTEND를 반복한다.
- `SWAP`: start/goal tree 역할을 번갈아 바꾼다.

## 성질을 혼동하지 않기

- probabilistic complete: 충분한 무한 시간이 주어질 때 해를 찾을 확률이 1로 수렴한다.
- resolution complete: 특정 격자 해상도에서 완전하다는 뜻이며 RRT-Connect의 기본 성질과 다르다.
- optimal: 비용이 최적값으로 수렴한다는 뜻이며 기본 RRT-Connect는 이를 목표로 하지 않는다.
- bounded runtime: 제품이 iteration/time/node 상한을 추가한 성질이다. 이때 budget 안의 성공은 보장되지 않는다.

## 튜닝과 검증

- step이 너무 크면 collision edge가 자주 막히고, 너무 작으면 node/nearest 비용이 커진다.
- collision resolution이 거칠면 얇은 장애물을 통과하는 false negative가 생긴다.
- 좁은 통로 성공률은 seed 여러 개와 고정 time budget으로 통계화한다.
- 성공 경로는 shortcut/smoothing/time parameterization 뒤 연속 충돌과 동역학 limit을 다시 검사한다.
- scene version이 planning 시작/종료/실행 사이에 바뀌었는지 확인한다.

원 논문: Kuffner & LaValle, [RRT-Connect](https://www.clear.rice.edu/comp450/papers/kuffner_lavalle_00.pdf), ICRA 2000.
