# Visual Odometry 핵심 구조

## VO와 SLAM의 경계

Visual Odometry(VO)는 연속 camera 관측으로 국소 motion을 누적한다. SLAM은 여기에 지속적인 map, loop closure, relocalization, global consistency를 더한다. Frame-to-frame pose가 잘 나오는 데모를 곧바로 SLAM 또는 global localization이라고 부르면 drift와 recovery 요구가 숨는다.

## 대표 geometry

- **Monocular 2D–2D:** Essential matrix로 rotation과 translation 방향을 얻지만 절대 scale은 없다.
- **Stereo/RGB-D/LiDAR-assisted 3D–2D:** Metric 3D point와 image observation으로 PnP를 풀어 scale을 관찰한다.
- **3D–3D:** Depth가 있는 대응점 두 집합을 SVD/Kabsch/Procrustes로 정합한다.
- **Direct method:** Feature 대신 image intensity/photometric residual을 최적화한다.

평면 로봇에서 metric 2D point 쌍 `p_prev`, `p_cur`를 쓸 때는

\[
\min_{R,t}\sum_i\|p_i^{prev}-(Rp_i^{cur}+t)\|^2
\]

를 푼다. 중심을 제거한 뒤 cross/dot 합의 `atan2`로 회전을 구하고 `t=c_prev-Rc_cur`로 병진을 복원할 수 있다.

## Robustness 계층

1. Descriptor/track 품질과 mutual match
2. Timestamp/extrinsic/intrinsic 계약
3. Geometric model gate(essential matrix, PnP, epipolar/reprojection error)
4. Bounded RANSAC 또는 robust kernel
5. Pose covariance/quality score와 tracking state
6. Keyframe/local BA
7. Loop closure와 relocalization
8. 독립 safety monitor와 stale-pose timeout

고정 residual threshold 하나만으로 모든 속도, 거리, feature noise를 다룰 수 없다. Pixel/metric uncertainty, depth, motion prediction에 따라 정규화한 residual과 minimum inlier/spatial distribution 계약이 필요하다.

## Degenerate case

- Pure rotation 또는 translation 부족으로 monocular initialization 불가
- Feature가 거의 일직선/평면 한 곳에 몰림
- 반복 texture로 잘못된 match가 다수 발생
- 움직이는 물체가 정적 배경보다 우세
- Motion blur, rolling shutter, exposure 변화
- Camera–IMU/LiDAR time offset과 extrinsic drift

## System 설계 원칙

- Tracking deadline과 local/global optimization을 다른 실행 경계로 분리한다.
- Map point/keyframe 생성 정책과 culling 정책을 함께 설계한다.
- Tracking lost/relocalizing/recovered 상태를 controller에 명시적으로 전달한다.
- Dataset 평균 FPS가 아니라 target workload의 latency tail과 failure recovery를 검증한다.
- Pose frame, timestamp, covariance, scale observability를 interface contract에 넣는다.

## 참고

- [ORB-SLAM — IEEE TRO 2015](https://doi.org/10.1109/TRO.2015.2463671)
- [ORB-SLAM 공개 원고](https://arxiv.org/abs/1502.00956)
- [ORB-SLAM 저자 구현](https://github.com/raulmur/ORB_SLAM)
