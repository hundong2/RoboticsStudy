# Frontier-Based Exploration

## 핵심 정의

Frontier는 현재 지도에서 **free cell이 unknown cell과 맞닿은 경계**다. free 쪽 경계로 이동하면 안전하게 알려진 공간을 따라가면서 센서 시야를 unknown 영역으로 확장할 수 있다.

\[
\mathcal{F}=\{i\mid m_i=free \land \exists j\in\mathcal{N}(i):m_j=unknown\}
\]

## 기본 파이프라인

1. occupancy/evidence grid 갱신
2. frontier cell 검출
3. connected component clustering과 작은 cluster 제거
4. 각 후보의 information gain, path cost, risk 평가
5. 도달 가능한 목표 할당
6. 주행 중 progress/stuck 감시와 재계획

## 대표 utility

\[
U(C)=w_i I(C)-w_c Cost(C)-w_r Risk(C)-w_s SwitchPenalty(C)
\]

`I(C)`는 ray casting/entropy로 예상한 정보 이득, `Cost(C)`는 global planner 경로 비용, `Risk(C)`는 localization/traversability 위험, `SwitchPenalty(C)`는 목표 흔들림을 막는 hysteresis다. cluster cell 수는 `I(C)`의 값싼 proxy일 뿐이다.

## 흔한 실패

- centroid가 occupied/unknown 또는 장애물 반대편에 놓임
- inflation 후에는 도달 불가능한 좁은 frontier
- map noise가 만든 작은 frontier를 반복 선택
- SLAM loop closure 뒤 좌표가 이동했는데 stale goal 유지
- 여러 로봇이 같은 frontier를 중복 선택

후보점을 반드시 실제 free cell로 snap하고, Nav2 global planner로 reachability를 확인하며, goal TTL/blacklist/progress timeout을 둔다.

## 근간 논문

- [B. Yamauchi, A Frontier-Based Approach for Autonomous Exploration, CIRA 1997](https://doi.org/10.1109/CIRA.1997.613851)
