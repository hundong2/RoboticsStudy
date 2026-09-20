# 논문 리뷰 — LOAM: Lidar Odometry and Mapping in Real-time

## 서지 정보

- Ji Zhang, Sanjiv Singh, *LOAM: Lidar Odometry and Mapping in Real-time*
- Robotics: Science and Systems (RSS), 2014
- DOI: [10.15607/RSS.2014.X.007](https://doi.org/10.15607/RSS.2014.X.007)
- 원문/메타데이터: [CMU Robotics Institute](https://www.ri.cmu.edu/publications/loam-lidar-odometry-and-mapping-in-real-time/)

이 논문은 오늘 기준의 “최신 모델”이 아니라, 움직이는 3D LiDAR의 왜곡·odometry·mapping을 실시간 파이프라인으로 분해한 근간 연구다. 오늘 실습은 그중 **측정 시각이 다른 광선을 공통 좌표계에 놓는 문제**만 2D로 축소한다.

## 1. 해결하려는 문제

회전식 LiDAR 한 sweep은 한순간의 사진이 아니다. 첫 점과 마지막 점 사이에 로봇이 이동·회전하므로, 한 포즈에서 동시에 측정했다고 가정하면 벽과 모서리가 휘어진다. 동시에 모든 점, 여러 scan, 6-DoF trajectory를 한꺼번에 고정밀 최적화하면 온라인 계산량이 너무 커진다.

LOAM의 질문은 실무적으로 명확하다.

> 고정밀 IMU나 무거운 offline batch 처리에만 의존하지 않고, 움직이는 LiDAR에서 낮은 drift의 odometry와 일관된 map을 실시간으로 만들 수 있는가?

motion distortion, scan registration, map 정합이 서로 얽혀 있다는 것이 어려움의 핵심이다. 잘못된 운동 추정은 점군을 왜곡하고, 왜곡된 점군은 다음 운동 추정을 다시 악화시킨다.

## 2. 핵심 아이디어와 수학적 직관

### 빠른 odometry와 느린 mapping의 역할 분리

LOAM은 큰 최적화 하나를 두 주기로 나눈다.

- **LiDAR odometry:** 높은 주기, 낮은 fidelity로 scan 사이의 운동을 빠르게 추정하고 sweep 왜곡을 보정한다.
- **LiDAR mapping:** 약 한 자릿수 낮은 주기에서 더 많은 feature와 반복을 사용해 scan-to-map 정합을 정밀화한다.
- **Transform integration:** 두 결과를 합쳐 높은 주기의 최종 pose를 제공한다.

이 분리는 “모든 계산을 빠르게”가 아니라, 빠른 피드백 경로와 정밀 보정 경로의 시간 예산을 다르게 둔다는 아키텍처 결정이다. 현대 로봇 파이프라인에서도 front-end/back-end 분리, fast loop/slow loop 분리로 반복되는 패턴이다.

### edge와 plane feature

모든 점을 같은 가치로 쓰지 않고 scan line의 곡률이 큰 sharp edge와 곡률이 작은 planar surface를 고른다. edge point `p`와 대응 line의 두 점 `a,b` 사이 거리는 다음 직관을 갖는다.

```text
d_edge = ||(p-a) × (p-b)|| / ||a-b||
```

분자는 두 벡터가 만드는 평행사변형 넓이이고, 밑변 길이로 나누면 높이, 즉 point-to-line 거리가 된다. plane point는 법선 `n`과 평면 위 점 `a`로 다음 residual을 만든다.

```text
d_plane = |(p-a) · n|
```

현재 pose를 조금 바꾸었을 때 이 residual들이 줄어들도록 반복 최적화한다. odometry 경로는 빠른 correspondence를, mapping 경로는 주변 점들의 기하 분포를 더 꼼꼼히 확인한다.

### 오늘 코드와의 연결

오늘 코드는 odometry를 추정하지 않고 시뮬레이터가 제공한 TF 시작/끝 pose를 사용한다. `i`번째 광선의 `alpha=i/(N-1)`에 대해 translation은 선형 보간하고 yaw는 최단 각도로 보간한다. 그 pose와 `base_link→laser` extrinsic을 합성해 endpoint를 map에 놓는다.

즉, LOAM의 6-DoF feature registration을 재현하는 코드가 아니다. LOAM 앞단에서 반드시 해결해야 하는 **rolling acquisition time과 pose의 결합**을 분리해 학습하는 최소 예제다.

## 3. 실무 적용 가능성과 한계

### 적용할 만한 설계 원리

1. 센서 메시지의 timestamp를 “도착 시각”이 아니라 측정 시각으로 다룬다.
2. 빠른 motion estimate와 느린 high-fidelity map update의 주기·executor·예산을 분리한다.
3. 모든 raw point보다 기하학적으로 정보가 큰 feature를 선별해 계산 예산을 집중한다.
4. deskew, odometry, mapping 각각의 출력과 지연을 독립 계측해 어느 단계가 drift를 만드는지 분해한다.

### 제품화할 때 주의할 한계

- **loop closure 부재:** 원 논문 파이프라인만으로 장거리 누적 drift를 전역적으로 제거하지 않는다. 별도의 place recognition/pose graph가 필요하다.
- **퇴화 환경:** 긴 복도, 평면만 있는 장소, 반복 구조에서는 관측 가능한 운동 축이 줄어든다.
- **동적 물체:** 자동차·사람의 edge/plane이 정적 map feature로 섞이면 correspondence가 오염된다.
- **시간·외부 파라미터:** per-point timestamp, IMU/LiDAR time offset, `base_link→lidar` extrinsic이 틀리면 정교한 optimizer도 일관된 map을 만들 수 없다.
- **deskew의 순환 의존:** deskew에는 pose가 필요하고 pose 추정에는 deskew된 점군이 유리하다. 초기화와 반복 전략이 중요하다.
- **실시간의 의미:** 평균 처리 주기가 센서 주기를 따라간다는 결과와 hard deadline/WCET 보장은 다르다. allocator, DDS, OS scheduler까지 포함한 최악 지연을 별도 검증해야 한다.

오늘 실습의 2D 선형 보간은 짧고 부드러운 운동에서는 유용하지만 급격한 회전·가속, 3D roll/pitch, time offset, pose covariance의 상관관계를 생략한다. `sigma_end` 가중치도 설명 가능한 휴리스틱이며 LOAM의 feature optimizer나 엄밀한 Bayesian map update를 대신하지 않는다.

## 엔지니어 체크리스트

- 드라이버가 `header.stamp`를 첫 점/마지막 점/packet 시각 중 무엇으로 정의하는가?
- `time_increment` 또는 PointCloud2의 per-point time field가 실제 firing order와 일치하는가?
- TF cache가 scan 시작부터 끝까지 포함하며 extrapolation을 숨기고 있지 않은가?
- deskew 전후 point-to-plane residual, 벽 두께, callback latency를 같은 bag에서 비교했는가?
- 급가속·정지·회전·동적 물체·TF dropout에서 fail-safe 동작을 정의했는가?
