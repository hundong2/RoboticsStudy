# 논문 리뷰 — Immediate-Update MSCKF

## 서지 정보

- Qingchao Zhang, Wei Ouyang, Jiale Han, Qi Cai, Maoran Zhu, Yuanxin Wu
- **“An Immediate Update Strategy of Multi-State Constraint Kalman Filter for Visual-Inertial Odometry”**
- *IEEE Robotics and Automation Letters*, 10(4), 4125–4131, 2025
- IEEE 공개일: 2025-03-10, DOI: [10.1109/LRA.2025.3549664](https://doi.org/10.1109/LRA.2025.3549664)
- 공개 원고: [arXiv:2411.02028](https://arxiv.org/abs/2411.02028)
- 배경이 되는 근간 논문: Mourikis & Roumeliotis, [“A Multi-State Constraint Kalman Filter for Vision-aided Inertial Navigation,” ICRA 2007](https://doi.org/10.1109/ROBOT.2007.364024)

## 1. 해결하려는 문제

MSCKF는 IMU 상태와 여러 과거 카메라 자세(clone)를 EKF 상태에 유지한다. 하나의 3D 특징점이 여러 카메라 자세에서 관측되면, 그 특징점 자체를 필터 상태에 계속 넣지 않고도 자세들 사이의 기하 제약을 만들 수 있다. 이 구조는 특징점 수에 대한 계산량을 억제해 임베디드 VIO에서 매력적이다.

전통적인 MSCKF 구현은 보통 특징점 추적이 끝날 때까지 관측을 모은 뒤 한 번에 업데이트한다. 문제는 다음과 같다.

1. 추적이 길게 유지되는 동안 최신 영상 정보가 상태에 반영되지 않아 보정 지연이 생긴다.
2. EKF는 비선형 측정식을 현재 추정값 주변에서 선형화한다. 업데이트가 늦으면 그 선형화점 자체가 오래된 상태에 머물러 오차가 커질 수 있다.
3. 긴 track 하나에 의존하는 지연 업데이트는 관측 수가 적거나 track이 불규칙한 상황에서 쓸 수 있는 제약과 업데이트 기회를 놓친다.

논문의 질문은 단순하다. **“특징점을 잃을 때까지 기다리지 않고, 지금 확보된 관측으로 안전하게 MSCKF 업데이트할 수 있는가?”**

## 2. 핵심 아이디어와 수학적 직관

### MSCKF의 출발점: 특징점을 상태에서 제거하기

한 특징점의 선형화된 영상 residual을 단순화하면 다음과 같다.

```text
r ≈ H_x δx + H_f δp_f + n
```

- `δx`: IMU와 복제한 카메라 자세의 오차 상태
- `δp_f`: 3D 특징점 위치 오차
- `H_x`, `H_f`: 각각에 대한 measurement Jacobian

`H_f`의 left null space 기저 `N`을 골라 `N^T H_f = 0`이 되게 하면,

```text
N^T r ≈ N^T H_x δx + N^T n
```

이 된다. 즉 3D 특징점 오차 항은 사라지고 카메라 자세 사이 제약만 남는다. 특징점을 영구 상태로 키우지 않으면서 영상 정보를 EKF에 넣는 것이 MSCKF의 핵심이다.

### 지연 업데이트의 약점

고전 방식은 한 feature track의 마지막까지 관측을 모은다. 관측 수가 많아져 삼각측량은 안정될 수 있지만, 그동안 필터의 선형화점은 새 영상 보정을 받지 않는다. 비선형 시스템에서 오차 상태가 커지면 `h(x)`를 1차 근사한 `H`의 품질도 떨어진다. 이것은 단순한 latency 문제라기보다 **다음 Jacobian을 계산하는 기준 상태의 품질 문제**다.

### 논문의 변화: 현재 관측으로 즉시 제약 만들기

저자들은 현재까지의 관측으로 3D 특징점과 측정 제약을 적시에 재구성해, feature가 사라지기 전에도 더 자주 필터 업데이트를 수행하는 전략을 분석한다. 직관은 다음 반복이다.

```text
새 관측 도착
  → 현재까지의 multi-view geometry로 제약 구성
  → EKF 상태를 즉시 보정
  → 더 나은 상태에서 다음 측정 Jacobian 계산
```

업데이트가 빨라지면 이용 가능한 제약과 보정 횟수가 늘고, 다음 선형화점이 실제 궤적에 더 가까워질 가능성이 높다. 논문은 시뮬레이션과 실제 데이터 실험에서 적은 특징 관측만 있는 경우에도 이 전략이 기존 지연 업데이트보다 정확도를 높일 수 있음을 보고한다.

여기서 중요한 점은 “같은 측정을 자주 재사용하면 좋다”가 아니다. 이미 사용한 정보의 상관관계를 무시해 중복 반영하면 공분산이 거짓으로 작아진다. 즉시 업데이트의 가치는 **새로 유효해진 독립 제약을 일관되게 구성하고, 더 좋은 선형화점을 다음 단계에 제공하는 것**에 있다.

## 3. 실무 적용 가능성과 한계

### 어디에 유용한가

- 드론과 이동로봇처럼 VIO 출력 latency가 곧 제어 성능으로 이어지는 시스템
- CPU/메모리 예산 때문에 대규모 bundle adjustment보다 filter 기반 VIO가 유리한 엣지 장치
- 특징 track 길이가 들쭉날쭉하고, 빠른 상태 보정이 다음 feature projection/gating에도 도움을 주는 환경
- GNSS 음영, 실내, 지하처럼 카메라+IMU가 주 위치 추정원이 되는 제품

오늘의 평면 EKF는 MSCKF 구현이 아니지만 같은 설계 감각을 연습한다. `/gps/position`이 들어오면 큐에 오래 쌓아두지 않고 바로 Correct를 수행한다. 그러면 다음 `/wheel/twist` Predict는 보정된 `x,y,yaw`에서 Jacobian `F`를 계산한다.

### 제품화 전에 확인할 한계

1. **초기 시차(parallax):** 관측이 너무 적거나 카메라가 거의 회전만 하면 즉시 삼각측량한 깊이가 불안정할 수 있다. condition number, depth 양수 조건, parallax gate가 필요하다.
2. **데이터 상관관계:** 같은 feature 관측을 여러 업데이트에 잘못 중복 사용하면 EKF가 과신한다. 논문의 측정 구성과 marginalization 규칙을 정확히 구현해야 한다.
3. **선형화 일관성:** 업데이트를 자주 해도 observability와 gauge freedom 문제가 저절로 해결되지는 않는다. FEJ 또는 invariant formulation 같은 일관성 기법이 별도로 필요할 수 있다.
4. **프런트엔드 의존성:** 잘못된 feature match, rolling shutter, 카메라-IMU 외부 파라미터/시간 동기 오차는 backend 개선보다 큰 오차를 만들 수 있다.
5. **계산량의 위치:** 최종 큰 업데이트를 나눌 뿐 전체 비용이 공짜가 되는 것은 아니다. 더 잦은 triangulation, Jacobian, null-space projection의 worst-case latency를 측정해야 한다.
6. **평가 범위:** 공개 데이터셋 개선이 모든 렌즈, 진동, 조도, motion profile을 대표하지 않는다. 목표 하드웨어에서 ATE뿐 아니라 update latency, CPU peak, innovation NIS와 3σ consistency를 함께 본다.

## 엔지니어의 결론

이 논문의 핵심은 “더 최신 알고리즘”이라는 이름보다 **측정 반영 시점도 estimator 설계 변수**라는 점이다. EKF의 계산량을 줄이기 위해 업데이트를 미루면 latency뿐 아니라 다음 선형화 품질까지 잃을 수 있다. 반대로 즉시 업데이트는 더 많은 상태 수정 기회를 주지만, 관측 독립성·삼각측량 품질·worst-case 실행시간을 함께 관리해야 한다.

실무 체크리스트는 다음 네 문장으로 압축된다.

1. feature track이 끝나야만 update 가능한 구조인지 확인한다.
2. update 전후의 Jacobian 선형화점을 로그로 남긴다.
3. ATE 개선과 함께 NIS/NEES 또는 3σ 일관성을 본다.
4. 평균 FPS가 아니라 update callback의 p99와 최대 실행시간을 잰다.
