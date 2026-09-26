# 논문 리뷰 — FAST-LIO2: Fast Direct LiDAR-Inertial Odometry

## 서지 정보

- Wei Xu, Yixi Cai, Dongjiao He, Jiarong Lin, Fu Zhang
- *IEEE Transactions on Robotics*, 38(4), 2053–2073, 2022
- DOI: [10.1109/TRO.2022.3141876](https://doi.org/10.1109/TRO.2022.3141876)
- 공개 원문: [arXiv:2107.06829](https://arxiv.org/abs/2107.06829)

이 논문은 오늘 실습처럼 IMU로 LiDAR scan을 시간 보정하는 데서 출발하지만, 그 결과를 지도와 직접 정합하고 다시 상태를 갱신하는 완전한 LiDAR-inertial odometry로 확장한다.

## 1. 해결하려는 문제

기존 LiDAR odometry는 흔히 scan에서 edge/plane feature를 먼저 고른 뒤 지도에 정합한다. 이 방식은 회전형 다중 채널 LiDAR에는 잘 맞지만 다음 문제가 있다.

1. solid-state LiDAR처럼 scan pattern이 불규칙하거나 FoV가 작으면 수작업 feature 규칙이 깨지기 쉽다.
2. feature extraction이 버린 raw point에도 자세를 구속하는 약한 정보가 있을 수 있다.
3. 매 scan마다 커지는 지도를 일반 kd-tree로 갱신·삭제·재균형화하면 실시간 비용이 커진다.
4. 빠른 회전에서는 scan 내부 점마다 자세가 달라 deskew와 상태 추정이 강하게 결합된다.

즉, 목표는 특정 LiDAR pattern에 덜 의존하면서 raw point를 충분히 활용하고, 큰 지도를 계속 갱신해도 onboard 계산 예산을 지키는 LIO다.

## 2. 핵심 아이디어와 수학적 직관

### 2.1 tightly-coupled iterated error-state Kalman filter

상태에는 위치, 속도, 자세, IMU bias, 중력, LiDAR-IMU extrinsic 등이 포함된다. IMU는 짧은 시간 간격의 motion prior를 예측하고, LiDAR point-to-plane residual이 그 예측을 교정한다.

한 점을 현재 상태 `x`로 지도 좌표에 옮긴 뒤 근처 평면 `(n,q)`와 비교하면 잔차는 직관적으로 다음과 같다.

\[
r_i(x)=n_i^\top\left(p_i^{map}(x)-q_i\right)
\]

자세가 바뀌면 모든 점의 지도 위치와 대응 평면이 함께 바뀌므로 한 번의 선형화로 끝내지 않고, 같은 측정 안에서 상태를 반복 선형화·갱신한다. 이것이 `iterated`의 핵심이다. 오늘 실습은 `R_e^T R_i p_i`까지만 계산하고 이 LiDAR measurement update는 하지 않는다.

### 2.2 feature 없이 raw point를 지도에 직접 정합

FAST-LIO2는 edge/plane feature를 별도 추출하는 대신 downsampled raw point를 지역 지도에 직접 정합한다. 센서별 feature 규칙을 줄이고 미세한 기하 정보를 더 보존하는 장점이 있다. 다만 “featureless”가 “전처리 없음”을 뜻하지는 않는다. 시간 보정, 유효점 선택, 이웃 탐색, 평면 적합, 이상치 억제는 여전히 중요하다.

### 2.3 ikd-Tree

지도는 매 scan마다 점 삽입, 오래되거나 불필요한 점 삭제, voxel downsampling, 이웃 검색을 동시에 요구한다. 정적 kd-tree를 매번 다시 만들면 비용이 크다. ikd-Tree는 증분 갱신과 동적 재균형화를 지원해 이 지도 유지 비용을 낮춘다. 논문은 다양한 공개 데이터 19개 sequence와 실제 실험을 제시하고, 초록에서 최대 100 Hz odometry/mapping과 1000 deg/s 회전 사례를 보고한다. 이는 특정 하드웨어·데이터에서의 결과이며 hard deadline 보장은 아니다.

## 3. 실무 적용 가능성

- **다양한 LiDAR:** feature 규칙 의존도가 낮아 spinning/solid-state 센서 간 이식성이 좋다.
- **고속 기동:** IMU 예측과 point-level deskew가 빠른 회전에서 scan 왜곡을 줄인다.
- **대규모 online map:** incremental map 자료구조가 insertion/delete/downsample의 반복 비용을 관리한다.
- **오늘 코드의 다음 단계:** 현재 6×6 `[theta,bias]`를 위치·속도·중력·extrinsic까지 확장하고, deskew된 점의 point-to-plane Jacobian으로 ESIKF update를 추가하면 축소판 LIO 구조가 된다.

제품화할 때는 알고리즘 정확도만큼 다음 인터페이스가 중요하다.

- 드라이버별 point timestamp 단위/기준과 IMU clock 동기화
- LiDAR-IMU extrinsic의 온라인/오프라인 보정 및 변경 감지
- 초기 정지 실패, degeneracy, map corruption, 센서 dropout 상태 머신
- CPU affinity, allocator/RMW, map rebuild spike의 tail latency 계측
- localization confidence를 속도 제한/정지 정책으로 변환하는 독립 safety supervisor

## 4. 한계와 비판적 읽기

1. **퇴화 환경:** 긴 터널·평면 하나·반복 구조에서는 point-to-plane Jacobian의 관측 가능성이 떨어진다. IMU bias나 특정 축 drift가 지도 정합으로 충분히 교정되지 않을 수 있다.
2. **시간·외부 파라미터 오차:** deskew가 clock offset이나 extrinsic을 틀리게 쓰면 더 정교하게 잘못된 cloud를 만든다. covariance가 모델 밖 systematic error까지 자동으로 표현하지 않는다.
3. **동적 객체:** raw point를 많이 쓸수록 차량·사람처럼 움직이는 점도 많이 들어온다. robust loss, semantic/dynamic filtering이 별도 필요하다.
4. **평균 처리율과 hard RT의 차이:** 높은 Hz 보고는 유용하지만 WCET, page fault, priority inversion, DDS 지연의 상한 증명은 아니다.
5. **지도 메모리와 장시간 운용:** incremental tree도 무한한 자원이 아니다. 지역 지도 정책, 삭제 기준, loop closure/전역 일관성 전략이 필요하다.

## 오늘 실습과 논문의 경계

| 항목 | 오늘 실습 | FAST-LIO2 |
|---|---|---|
| 운동 | 회전만 | 6-DoF 위치·속도·자세 |
| IMU 상태 | gyro bias + 6×6 covariance | bias/중력/extrinsic을 포함한 확장 상태 |
| LiDAR 사용 | scan-end deskew | raw point-to-map 반복 update |
| 지도 | 고정 평면을 감사에만 사용 | ikd-Tree 증분 지도 |
| 목적 | 시간 계약과 bounded kernel 학습 | 실시간 odometry + mapping |

가장 중요한 교훈은 “deskew 전처리”와 “상태 추정”을 완전히 별개의 블랙박스로 보면 안 된다는 점이다. 점의 시각, IMU bias, 현재 자세, 지도 residual이 서로 영향을 주므로 제품에서는 이 경계를 명시적으로 설계하고 진단해야 한다.

