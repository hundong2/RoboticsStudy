# 논문 리뷰 — A Tutorial on Graph-Based SLAM

## 서지 정보

- Giorgio Grisetti, Rainer Kümmerle, Cyrill Stachniss, Wolfram Burgard
- *IEEE Intelligent Transportation Systems Magazine*, 2(4), 31–43, 2010
- DOI: [10.1109/MITS.2010.939925](https://doi.org/10.1109/MITS.2010.939925)
- 저자 소속 기관 저장소의 [서지·초록 페이지](https://iris.uniroma1.it/handle/11573/137105)

이 논문은 최신 leaderboard 경쟁 논문이 아니라, graph-based SLAM을 직접 구현할 수 있을 정도로 front-end 결과를 graph와 least-squares 문제로 연결해 설명한 근간 tutorial이다. 오늘 코드는 논문의 전체 sparse solver가 아니라 **작은 SE(2) graph의 핵심 정상방정식**을 재현한다.

## 1. 해결하려는 문제

Wheel odometry나 scan matching의 작은 오차는 이동할수록 누적된다. 로봇이 출발 장소로 돌아와 같은 장소임을 다시 알아보더라도, 단순히 현재 pose 하나만 출발점으로 끌어당기면 중간 궤적과 지도가 꺾인다. 필요한 것은 과거 keyframe 전체를 함께 조정하면서 다음 두 종류의 제약을 최대한 만족하는 일관된 궤적이다.

- 연속 pose 사이의 odometry/scan-matching 제약
- 멀리 떨어진 시각의 같은 장소를 연결하는 loop-closure 제약

Graph 표현에서 vertex는 미지의 robot pose, edge는 두 pose 사이 상대변환 측정이다. 문제는 모든 edge의 불일치를 불확실도에 맞게 최소화하는 vertex 배치를 찾는 것이다.

## 2. 핵심 아이디어와 수학적 직관

Pose `x_i`, `x_j`와 상대 측정 `z_ij`가 있을 때 예측 상대변환은 `h(x_i,x_j)=T_i^{-1}T_j`다. Edge error를

\[
e_{ij}(x)=h(x_i,x_j)\ominus z_{ij}
\]

로 두고, 측정 신뢰도의 역공분산인 정보행렬 `Ω_ij`로 가중하면 전체 목적함수는

\[
F(x)=\sum_{(i,j)} e_{ij}(x)^T\Omega_{ij}e_{ij}(x)
\]

가 된다. 현재 추정치 주변에서 `e(x+Δx)≈e(x)+JΔx`로 선형화하면 Gauss–Newton 정상방정식

\[
H\Delta x=-b,\qquad H=\sum J^T\Omega J,\quad b=\sum J^T\Omega e
\]

을 얻는다. `Δx`를 풀어 모든 pose를 조금씩 움직이고 다시 선형화하는 과정을 반복한다.

직관적으로 `H`는 어떤 pose 쌍이 얼마나 강하게 연결됐는지를 담는 가중 spring network와 비슷하다. Loop closure라는 새 spring을 걸면 끝점만 순간이동하지 않고, odometry spring의 신뢰도와 graph 연결 구조에 따라 오차가 경로 전체에 분배된다.

첫 pose를 고정하는 이유도 중요하다. 모든 pose를 똑같이 평행이동·회전해도 상대변환 error는 변하지 않으므로 전역 기준이 없으면 해가 무한히 많다. 이를 gauge freedom이라 하며, 오늘 코드는 pose 0을 변수에서 제외해 제거한다.

## 3. 실무 적용 가능성과 한계

### 바로 적용할 수 있는 것

- LiDAR/visual odometry를 인접 edge로, place recognition 결과를 loop edge로 통합하는 back-end 설계
- Edge별 covariance에서 정보행렬을 만들어 센서 신뢰도를 반영하는 방식
- Pose graph 최적화 전후 closure residual, iteration time, solver 실패를 운영 지표로 만드는 방법
- Front-end, optimizer, 독립 auditor를 ROS 2 topic과 SROS2 enclave로 분리하는 배포 구조

### 논문과 오늘 예제의 한계

- **Loop closure 오검출:** Least-squares는 잘못된 edge도 진실로 믿는다. 실제 시스템은 geometric verification와 robust kernel/switchable constraint가 필요하다.
- **선형화 의존성:** 초기 추정이 나쁘면 local minimum이나 발산이 가능하다. 큰 회전 오차에는 재선형화, 좋은 initialization, trust-region 계열이 필요하다.
- **Dense solver:** 오늘 코드는 교육용 최대 36변수 dense Cholesky다. 실제 graph는 sparse block structure, ordering, incremental solver를 사용해야 한다.
- **SE(2) 단순화:** 3D 로봇은 SE(3) Lie group, 6-DoF uncertainty, IMU bias와 시간 동기화를 다뤄야 한다.
- **고정 loop 가정:** simulator의 마지막 keyframe이 첫 keyframe과 정확히 같은 장소라는 ground truth를 알고 있다. 제품에서는 front-end가 loop 후보와 상대변환·공분산을 추정해야 한다.
- **RT 보장 아님:** 반복/메모리 상한은 있지만 DDS 수신, Path vector, 로그, OS scheduling까지 hard real-time이 된 것은 아니다.

## 엔지니어가 가져갈 한 문장

Pose-graph SLAM의 핵심은 “누적 drift를 지우는 필터”가 아니라, **서로 다른 시각의 상대측정들을 하나의 가중 비선형 least-squares 문제로 만들고 전역 일관성을 다시 계산하는 것**이다.
