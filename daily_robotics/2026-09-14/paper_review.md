# 논문 리뷰 — Elastic Bands: Connecting Path Planning and Control

## 서지 정보

- **저자:** Sean Quinlan, Oussama Khatib
- **학회:** 1993 IEEE International Conference on Robotics and Automation (ICRA)
- **페이지:** 802–807, Vol. 2
- **DOI:** [10.1109/ROBOT.1993.291936](https://doi.org/10.1109/ROBOT.1993.291936)
- **원문:** [Stanford Robotics Lab PDF](https://khatib.stanford.edu/publications/pdfs/Quinlan_1993_ICRA.pdf)

이 논문은 최신 논문 대신 오늘 알고리즘의 뿌리를 이해하기 위해 고른 **근간 논문**이다. 현대 local planner가 전역 경로를 그대로 추종하지 않고 센서 변화에 맞춰 짧은 구간을 계속 다듬는 발상의 출발점을 보여 준다.

## 1. 해결하려는 문제

전역 path planner는 시작점에서 목표점까지 막히지 않는 길을 찾지만, 보통 계산 비용이 크고 센서가 새 장애물을 볼 때마다 즉시 다시 호출하기 어렵다. 반대로 반응형 controller는 빠르지만 눈앞의 장애물만 피하다 막다른 곳에 갇힐 수 있다.

논문의 질문은 명확하다.

> “전역 계획의 목표 지향성을 유지하면서, 국소 센서 변화에는 제어 주기 가까운 속도로 반응할 수 없을까?”

저자들은 전역 planner가 만든 충돌 없는 path를 버리지 않고, 고무 밴드처럼 실시간 변형되는 중간 표현을 둔다. 작은 환경 변화는 밴드가 흡수하고, 문이 완전히 막히는 식의 위상 변화만 전역 planner로 돌려보낸다.

## 2. 핵심 아이디어와 수학적 직관

### Path를 “bubble의 띠”로 본다

로봇 configuration마다 주변 자유 공간을 나타내는 bubble을 두고, 서로 겹치는 bubble의 열을 elastic band로 본다. 밴드가 끊어지지 않는 동안 시작과 목표의 연결성은 유지된다. 단순 point path보다 로봇 크기와 국소 여유 공간을 함께 생각하게 하는 표현이다.

오늘 실습은 2D 점 로봇에 원형 안전 반경을 합친 축소판이며, 각 knot에 두 종류의 힘을 적용한다.

### 내부 수축력: 불필요한 꺾임을 편다

```text
F_internal,i = k_c (p_(i-1) - 2 p_i + p_(i+1))
```

괄호 안은 경로의 이산 2차 미분이다. 직선 위에서는 거의 0이고 날카롭게 꺾인 곳에서 커진다. 따라서 이 힘을 반복 적용하면 밴드의 “늘어진 부분”이 줄고 경로가 매끄러워진다.

### 외부 반발력: 장애물 여유를 만든다

장애물 중심 `o`, knot와 장애물의 거리 `d`, 영향 거리 `d0`라 두면 오늘의 단순화된 힘은 다음과 같다.

```text
F_external,i = k_r (d0 - d) / d · (p_i - o) / d    if d < d0
               0                                    otherwise
```

`(p_i-o)/d`는 장애물 바깥 방향 단위 벡터다. 가까울수록 반발이 커지고 충분히 멀면 계산하지 않는다. 내부 힘이 경로를 짧게 당기고 외부 힘이 장애물에서 밀어내므로, 둘의 평형이 “짧고 부드럽지만 여유가 있는” 국소 경로가 된다.

### 왜 global planner를 완전히 대체하지 않는가

고무 밴드는 현재 path와 같은 위상 부류 안에서 연속적으로 변형하는 local method다. 원래 통과하려던 문이 닫히면 다른 복도로 순간 이동할 수 없다. 밴드가 끊기거나 안전 여유를 유지하지 못하면 실패를 감지하고 global replanning을 요청해야 한다. 오늘 BT의 `RecoveryNode → ComputePath` 연결이 바로 이 경계다.

## 3. 실무 적용 가능성

### 좋은 적용 지점

- 물류 AMR의 전역 경로를 사람·카트의 작은 위치 변화에 맞춰 빠르게 다듬기
- global planner와 velocity controller 사이의 local trajectory 계층
- 조작 로봇에서 이미 계산된 collision-free path의 작은 온라인 보정
- 계산량 상한이 필요한 embedded controller에서 bounded knot/window 최적화

### 오늘 구현에 옮긴 부분

- 전역 path의 시작/목표를 고정하고 내부 knot만 갱신한다.
- `std::array`로 knot 21개, 장애물 8개, 반복 8회를 고정한다.
- 각 segment에 `dt ≥ length / 0.6`을 부여해 timestamp가 단조 증가하는 궤적을 만든다.
- heartbeat를 독립 supervisor가 감시해 optimizer가 늦으면 0 속도를 강제한다.
- BT가 recovery와 global replanning의 책임 경계를 표현한다.

## 한계와 엔지니어링 경고

1. **국소 최솟값:** 인공 힘은 좁은 통로나 대칭 장애물에서 좋지 않은 평형에 머물 수 있다.
2. **위상 전환 불가:** 닫힌 문 뒤의 다른 복도처럼 전혀 다른 길은 global planner가 찾아야 한다.
3. **동역학 미포함:** 원 논문의 핵심은 path deformation이다. 오늘의 timestamp 부여도 속도/가속도/jerk를 함께 푸는 완전한 trajectory optimization이 아니다.
4. **연속 충돌 미검사:** knot가 안전해도 knot 사이 선분이나 실제 footprint가 충돌할 수 있다.
5. **동적 장애물 예측 부족:** 현재 위치 반발만으로는 사람의 미래 경로를 다루지 못한다.
6. **하드 RT 보장 아님:** 배열과 반복 상한은 분석 가능성을 높이지만, DDS·executor·OS scheduling의 WCET 측정과 admission test가 따로 필요하다.

## 제품 설계로 확장할 때

- costmap distance transform의 gradient로 외력을 계산한다.
- footprint swept volume과 time-to-collision을 제약에 넣는다.
- 비홀로노믹, 속도, 가속도, 곡률 제한을 sparse graph 최적화에 포함한다.
- band가 끊기거나 cost가 발산하면 명시적 failure를 BT blackboard에 기록한다.
- planner/controller/safety supervisor를 failure domain별 process와 CPU로 격리한다.
- 평균 시간 대신 p99.999 callback latency와 최악 장애물 수에서의 WCET를 검증한다.

## 한 문장 평가

Elastic Band의 가장 큰 가치는 특정 최적화 공식이 아니라, **전역 계획과 빠른 센서 기반 제어 사이에 계속 변형되는 경로 표현을 둔다**는 시스템 아키텍처다. 다만 그 경계를 넘는 변화는 반드시 global replanning과 독립 안전 계층으로 넘겨야 한다.
