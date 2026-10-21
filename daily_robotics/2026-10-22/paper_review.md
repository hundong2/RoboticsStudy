# 논문 리뷰 — Switchable Constraints for Robust Pose Graph SLAM

## 서지 정보

- Niko Sünderhauf, Peter Protzel
- IEEE/RSJ International Conference on Intelligent Robots and Systems (IROS), 2012, pp. 1879–1884
- DOI: [10.1109/IROS.2012.6385590](https://doi.org/10.1109/IROS.2012.6385590)
- 원문: [저자 공개 PDF](https://nikosuenderhauf.github.io/assets/papers/IROS12-switchableConstraints.pdf)

이 논문은 최신 논문 대신 **강건 graph-SLAM back-end의 근간 논문**으로 선정했다. 오늘 실습의 false-loop 격리 구조를 가장 직접적으로 설명한다.

## 1. 해결하려는 문제

Pose graph의 front-end는 “이 두 장소가 같다”는 loop closure 후보를 만든다. 후보가 맞으면 누적 odometry drift를 크게 줄이지만, 반복 복도·비슷한 외관·조명 변화 때문에 서로 다른 장소를 같은 장소로 오인하면 하나의 잘못된 edge가 전체 지도를 심하게 접을 수 있다.

당시의 일반적인 least-squares back-end는 graph topology, 즉 어떤 edge가 존재하는지를 이미 정답으로 간주했다. Huber 같은 residual별 robust cost도 도움이 되지만, 여러 개의 서로 일관된 false loop가 있거나 잘못된 edge가 pose까지 끌어당기면 충분하지 않을 수 있다. 논문은 “front-end가 data association을 틀릴 수 있다”는 사실을 back-end 문제 안으로 가져온다.

## 2. 핵심 아이디어와 수학적 직관

각 의심 가능한 loop edge `ij`에 숨은 연속 변수 `s_ij`를 하나 추가한다.

\[
X^*,S^*=\arg\min_{X,S}
\sum_i\|r_i^{odom}(X)\|^2
+\sum_{ij}\|\Psi(s_{ij})r_{ij}^{loop}(X)\|^2
+\sum_{ij}\|\gamma_{ij}-s_{ij}\|^2
\]

- `X`: robot pose들.
- `S`: loop마다 하나씩 붙는 switch 변수들.
- `Ψ(s)`: switch를 `[0,1]` 영향도로 바꾸는 함수.
- `γ≈1` prior: 처음에는 후보를 정상으로 믿되, residual이 매우 크면 끌 수 있게 한다.

직관은 전기 스위치와 비슷하다. 정상 loop는 작은 residual을 설명하므로 prior를 이기며 `s≈1`에 남는다. false loop는 켠 상태의 비용이 너무 크므로 prior 비용을 감수하고 `s≈0`으로 내려간다. 확률적으로 보면 switch가 loop information matrix를 거의 0으로 만들어 covariance를 무한히 키우는 것과 같다.

논문은 sigmoid보다 `Ψ(s)=s`, `0≤s≤1`인 선형 switch가 plateau가 적어 더 나은 수렴을 보였다고 설명한다. 오늘 코드도 이 선형형과 projection을 사용한다. 다만 논문 규모의 sparse graph 최적화 대신 최대 12 pose/3 loop의 dense 교육용 solver다.

저자들은 2D/3D 합성·실세계 데이터셋에서 평가했다. 예를 들어 Manhattan graph는 3,500 pose와 2,099 loop를 가지며, 논문은 여러 데이터셋에서 최대 1,000개의 false loop가 추가된 경우까지 다룬 결과를 보고한다. 중요한 해석은 “1,000이면 항상 안전하다”가 아니라, graph topology를 최적화 변수로 만들면 단순 least squares보다 훨씬 큰 outlier 부하를 견딜 수 있다는 것이다.

## 3. 실무 적용 가능성

- Visual/LiDAR place-recognition 후보를 최종 graph에 넣기 전후의 방어층으로 쓸 수 있다.
- switch 값을 diagnostic으로 내보내면 특정 장소·계절·센서 조건에서 front-end가 반복적으로 실패하는지 추적할 수 있다.
- rosbag 회귀 테스트에서 known-bad loop를 주입하고 trajectory RMSE뿐 아니라 `false switch↓`, `true switch↑`를 함께 acceptance criterion으로 둘 수 있다.
- Huber, Dynamic Covariance Scaling, Max-Mixture, Graduated Non-Convexity 같은 다른 robust 방법과 비교할 기준점이 된다.

## 한계와 엔지니어링 주의점

1. **정보가 부족하면 잘못된 topology도 그럴듯할 수 있다.** 논문도 서로 약하게 연결된 parking level 사이에 false loop 그룹이 생긴 실패 사례를 제시한다. back-end만으로 물리적으로 불가능한 교차나 층 구조를 알 수 없다.
2. **prior covariance는 자유 파라미터다.** 너무 강하면 outlier가 pose를 망가뜨리고, 너무 약하면 유용한 loop를 모두 꺼서 odometry drift로 돌아간다.
3. **연산량이 늘어난다.** loop마다 변수와 prior factor가 추가된다. 장기 운영에는 sparse incremental solver, relinearization 정책, marginalization/graph pruning이 필요하다.
4. **front-end 검증을 대체하지 않는다.** descriptor score, RANSAC/ICP residual, visibility, time consistency를 먼저 확인해야 한다.
5. **Hard RT 보장이 아니다.** 반복 비선형 최적화의 수렴 횟수와 선형계 풀이 비용을 반드시 제한하고, deadline 초과 시 마지막 안전 추정치를 유지하는 정책이 필요하다.

## 오늘 코드와 논문의 차이

| 항목 | 논문 | 오늘 실습 |
|---|---|---|
| 상태 | 대규모 2D/3D pose graph | 12개 SE(2) pose |
| switch | 각 의심 loop의 숨은 변수 | 최대 3개, `[0,1]` projection |
| solver | graph 최적화 framework와 sparse 구조 | 최대 `36x36` dense 부분 피벗, 정확히 8회 |
| 입력 | 실제/benchmark graph | 반복 가능한 합성 drift + false loop 1개 |
| 검증 | 다수 데이터셋과 outlier 정책 | robust/naive Path의 독립 위치 RMSE |

따라서 오늘 결과는 논문을 재현한 benchmark가 아니라, **수식이 시스템 경계·고정 예산·ROS 진단으로 어떻게 내려오는지** 보여 주는 작은 실행 가능 뼈대다.
