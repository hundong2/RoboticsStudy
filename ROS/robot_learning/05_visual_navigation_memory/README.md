# 05. Visual Navigation, Long-Horizon, Mapless, Topological Memory

목표는 카메라 기반 navigation policy가 지도, memory, goal image, language command와 어떻게 결합되는지 이해하는 것입니다.

## 강의 목표

- Visual Navigation과 metric map 기반 Nav2의 차이를 설명한다.
- Mapless Navigation의 장점과 위험을 분석한다.
- Topological Map과 Memory를 이용한 long-horizon navigation을 설계한다.
- ViNT, GNM, NoMaD 계열 논문을 읽고 재현 계획을 만든다.

## 핵심 개념

| 개념 | 설명 |
|---|---|
| goal-conditioned navigation | 현재 이미지와 목표 이미지를 보고 action을 예측 |
| mapless navigation | 명시적 metric map 없이 observation에서 직접 행동 |
| topological map | 장소를 node, 이동 가능성을 edge로 표현 |
| memory | 과거 observation과 action을 저장해 현재 판단에 사용 |
| long-horizon navigation | 짧은 action만으로 해결되지 않는 장거리 목표 이동 |

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | Visual Navigation 문제 정의 | input/output schema |
| 2 | 이미지 goal matching | embedding distance plot |
| 3 | waypoint prediction | local waypoint model |
| 4 | topological graph 설계 | node/edge JSON |
| 5 | memory retrieval 구현 | nearest memory demo |
| 6 | route replay | stored route rollout |
| 7 | long-horizon subgoal planning | subgoal sequence |
| 8 | NoMaD/ViNT/GNM 논문 읽기 | paper summary |
| 9 | 평가 metric 설계 | success, SPL, collision |
| 10 | 실패 사례 분석 | perceptual aliasing report |

## 실습 과제

1. 이미지 파일 이름과 pose/action을 묶은 visual navigation toy dataset을 만든다.
2. 현재 이미지와 goal 이미지의 embedding distance를 계산한다.
3. topological memory graph를 만들고 nearest node를 검색한다.
4. closed-loop 평가 metric을 설계한다.

## 통과 기준

- Visual Navigation이 SLAM/Nav2를 완전히 대체하는 것이 아니라 다른 trade-off를 가진다는 점을 설명한다.
- Topological memory가 long-horizon 문제에서 필요한 이유를 설명한다.
- perceptual aliasing과 viewpoint change가 실패를 만드는 과정을 설명한다.
