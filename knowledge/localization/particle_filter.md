# Particle Filter와 Monte Carlo Localization

## 상태 표현

Particle filter는 posterior를 가중 sample 집합으로 근사한다.

```text
S_t = {(x_t^[i], w_t^[i]) | i=1..N}
```

MCL에서 `x=[x,y,yaw]`이고 한 update는 predict, correct, normalize, 필요 시 resample 순서다.

## Predict

각 particle을 `p(x_t|u_t,x_t-1)`에서 sampling한다. 차동구동이면 encoder의 `Δs, Δθ`를 적용하고 이동/회전에 비례하는 noise를 더한다. motion noise가 너무 작으면 실제 slip을 덮지 못하고, 너무 크면 scan마다 계산을 다시 수렴에 쓴다.

## Correct

```text
w_i ← w_i · p(z_t | x_i)
```

많은 beam likelihood를 곱하면 underflow가 생기므로 log domain에서 합하고 log-sum-exp로 정규화한다. 실제 Lidar에는 사람, 유리, max-range, map 불일치가 있으므로 단일 Gaussian보다 hit/random/max/outlier mixture와 beam skipping이 견고하다.

## Resampling과 퇴화

```text
N_eff = 1 / Σ_i w_i²
```

`N_eff`가 충분히 작을 때만 resample하면 불필요한 sample impoverishment를 줄인다. Systematic resampling은 난수 하나와 등간격 threshold를 써 multinomial 방식보다 낮은 분산과 `O(N)` 비용을 얻는다.

Resampling만으로 사라진 가설은 되살아나지 않는다. kidnapped robot recovery에는 random particle injection, global proposal, sensor-resetting distribution 같은 별도 메커니즘이 필요하다.

## 다봉 분포 출력

전체 particle 평균은 두 pose cluster 사이의 불가능한 위치가 될 수 있다. 제품에서는 다음을 함께 관리한다.

- dominant cluster pose와 weight
- 두 번째 cluster와 ambiguity ratio
- covariance/entropy/`N_eff`
- 마지막 유효 sensor update age
- map→odom jump 크기와 downstream acceptance 정책

yaw 평균은 선형 평균이 아니라 다음 원형 평균을 쓴다.

```text
mean_yaw = atan2(Σ w_i sin(yaw_i), Σ w_i cos(yaw_i))
```

## 시간·좌표계 계약

scan acquisition 시각의 `odom→base_link`와 sensor extrinsic을 사용해야 한다. 최신 TF를 무조건 쓰면 움직이는 로봇에서 measurement model과 pose 시각이 어긋난다. localization 계산이 늦어져도 motor safety loop를 막지 않도록 executor/process와 watchdog을 분리한다.

- [Fox et al., Monte Carlo Localization, CMU RI](https://publications.ri.cmu.edu/monte-carlo-localization-efficient-position-estimation-for-mobile-robots)
