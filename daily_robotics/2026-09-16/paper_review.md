# 논문 리뷰 — Unified Temporal and Spatial Calibration for Multi-Sensor Systems

- **저자:** Paul Furgale, Joern Rehder, Roland Siegwart
- **학회:** IEEE/RSJ International Conference on Intelligent Robots and Systems (IROS), 2013, pp. 1280–1286
- **DOI:** [10.1109/IROS.2013.6696514](https://doi.org/10.1109/IROS.2013.6696514)
- **서지 정보:** [ETH Research Collection](https://www.research-collection.ethz.ch/items/487b06bb-dcbe-411d-ab46-8580147273ac)

이 논문을 고른 이유는 multi-sensor fusion에서 공간 외부 파라미터만큼 중요한 **시간 외부 파라미터**를 별도 전처리 숫자가 아니라 최적화 변수로 다루는 근간을 보여 주기 때문이다.

## 1. 해결하려는 문제

카메라, IMU, LiDAR는 서로 다른 clock, sampling rate, driver queue를 쓴다. 센서 A의 timestamp와 센서 B의 timestamp가 같아도 실제로 같은 순간을 본다는 보장이 없다. 로봇이 움직일 때 이 시간 오차는 곧 공간 오차가 된다. 예를 들어 yaw-rate가 `1 rad/s`일 때 `35 ms` 오프셋은 약 `0.035 rad ≈ 2.0°`의 방향 불일치를 만든다.

전통적인 2단계 방식은 먼저 시간 오프셋을 고정한 뒤 공간 extrinsic을 추정한다. 첫 단계가 틀리면 두 번째 단계가 그 오차를 잘못된 회전/이동 extrinsic으로 흡수할 수 있다. 논문은 센서 간 공간 변환과 시간 오프셋을 하나의 최대우도 문제에서 함께 추정하는 것을 목표로 한다.

## 2. 핵심 아이디어와 수학적 직관

핵심 장치는 로봇의 궤적을 몇 개의 discrete pose만으로 두지 않고 **연속시간 함수**로 표현하는 것이다. 그러면 센서 `k`의 측정 시각에 offset `δ_k`를 더하거나 빼서도 궤적 pose를 평가할 수 있다.

개념적으로 센서 측정 `z_{k,i}`의 residual은 다음 꼴이다.

\[
r_{k,i}(\theta,\delta_k)=
z_{k,i}-h_k\!\left(x(t_{k,i}+\delta_k),\,T_{body}^{sensor_k}\right)
\]

- `x(t)`: 연속시간 로봇 궤적
- `T_body^sensor`: 공간 extrinsic
- `δ_k`: 센서 clock의 시간 offset
- `h_k`: 궤적과 extrinsic에서 해당 센서 관측을 예측하는 모델

전체 residual을 covariance로 whitening해 제곱합을 최소화하면, 궤적·공간 extrinsic·시간 offset이 서로 일관되는 해를 찾을 수 있다. 시간 offset이 바뀌면 단순히 measurement index를 옮기는 것이 아니라 연속 궤적 위 평가 시각이 부드럽게 이동하므로 gradient 기반 최적화에 넣을 수 있다는 점이 중요하다.

직관은 이렇다. 회전/가속이 충분히 풍부한 구간에서는 한 센서의 특징 변화와 다른 센서의 관성 변화가 같은 모양을 가진다. 두 파형을 시간축에서 밀어 residual이 가장 작아지는 위치가 offset 정보다. 동시에 spatial transform도 맞춰야 하므로 단순 cross-correlation보다 일반적인 sensor model을 수용한다.

## 3. 실무 적용 가능성

### 어디에 유용한가

- camera–IMU, LiDAR–IMU처럼 rate와 latency가 다른 sensor rig의 factory/offline calibration
- 데이터 수집 장비 교체 후 timestamp 경로가 바뀌었는지 검증
- SLAM/VIO residual이 커졌을 때 extrinsic 문제와 clock 문제를 함께 진단
- hardware trigger/PTP가 있어도 남는 exposure, scan, driver pipeline 지연의 보정

### 제품 적용 시 체크할 것

1. **관측 가능성:** 정지 또는 일정한 속도의 단조로운 운동만 있으면 시간 이동과 bias/spatial error를 구분하기 어렵다. 여러 축의 회전과 가속이 필요하다.
2. **stamp 정의:** camera exposure start/middle/end, LiDAR scan start/end 중 무엇인지 먼저 계약해야 한다. 정의가 흔들리면 constant offset 하나로 설명할 수 없다.
3. **clock skew:** 논문의 단순 offset 가정만으로 긴 기록의 서로 다른 clock rate까지 잡을 수 없다면 `δ(t)=δ₀+κt` 같은 drift 항이 필요하다.
4. **초기값/국소해:** joint nonlinear optimization은 초기 extrinsic과 offset, 반복 운동의 대칭성에 민감할 수 있다.
5. **계산/RT:** batch calibration의 정확도가 곧 online hard-RT 가능성을 뜻하지 않는다. production에서는 window 크기, iteration, marginalization, 실패 fallback을 별도 설계해야 한다.

## 오늘 실습과의 연결 및 차이

오늘 코드는 논문의 full joint spatiotemporal calibration을 재현하지 않는다. 교육용으로 회전 1축과 constant time offset만 남겨 다음처럼 축소했다.

- 연속시간 spline 대신 고속 IMU 표본의 선형 보간 사용
- 공간 extrinsic은 이미 맞았다고 가정
- `[-80,+80] ms`의 bounded grid search와 3점 포물선 보간 사용
- covariance-weighted residual로 후보 평가
- 보정 후 두 yaw-rate를 inverse-variance 방식으로 융합

이 축소판의 장점은 알고리즘과 실행량이 투명하다는 것이다. 단점은 3D extrinsic, sensor bias, outlier, non-constant latency, clock skew를 함께 추정하지 못한다는 것이다. 실제 카메라–LiDAR 제품에는 논문의 연속시간/최대우도 관점을 유지하되 robust loss, motion excitation 검사, calibration uncertainty, independent validation dataset이 추가되어야 한다.

## 엔지니어의 한 줄 결론

**Timestamp는 metadata가 아니라 추정해야 할 센서 파라미터일 수 있다.** 공간 보정만 반복하기 전에 시간축이 맞는지 residual과 motion excitation으로 먼저 확인하라.
