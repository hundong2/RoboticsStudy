# Learned Covariance Calibration과 OOD 경계

## 문제

학습 모델이나 EKF가 출력하는 분산/covariance는 “오차의 크기” 그 자체가 아니라 확률 모델의 주장이다. 평균 예측이 정확해도 분산이 너무 작으면 fusion gain, chance constraint, fault threshold가 모두 위험한 방향으로 움직인다.

## 최소 점검 세트

1. **Innovation**: `nu=z-h(x^-)`
2. **Innovation covariance**: `S=H P^- H^T+R`
3. **NIS**: `nu^T S^-1 nu`; truth 없이 measurement consistency를 본다.
4. **NEES**: `(x-x_hat)^T P^-1 (x-x_hat)`; ground truth가 있을 때 state consistency를 본다.
5. **Coverage**: 실제 error가 1/2/3-sigma 또는 confidence ellipse 안에 드는 비율을 본다.

RMSE와 calibration은 서로 대체하지 않는다. RMSE가 낮아도 covariance가 과신될 수 있고, 보수적인 covariance가 calibration score 일부를 개선하면서 제어 성능을 낮출 수도 있다.

## Positive-semidefinite map

matrix covariance를 직접 회귀하면 대칭/양의 준정부호 조건이 깨질 수 있다. network가 `L`을 출력하게 하고

```text
P_cal = L L^T + epsilon I
```

로 구성하면 최소한 algebraic validity를 확보한다. Cholesky factor의 diagonal을 `softplus`로 제한하는 방법도 흔하다.

## Calibration과 OOD는 분리한다

- Calibration은 “정상 운용 분포에서 confidence가 실제 빈도와 맞는가?”를 묻는다.
- OOD detection은 “현재 입력이 그 정상 분포에 속하는가?”를 묻는다.
- Residual/NIS gate는 “현재 measurement가 동역학 prediction과 맞는가?”를 묻는다.

공분산을 무한히 키우는 것으로 OOD 문제를 숨기지 않는다. OOD이면 update를 건너뛰고 fallback sensor/degraded mode로 전이하며, 별도 진단 카운터를 남긴다.

## 배포 체크리스트

- sensor serial/firmware/environment별 train-validation-test 분리
- bias와 timestamp/frame mismatch를 covariance inflation으로 덮지 않기
- offline coverage/NEES/NIS와 tail percentile 동시 보고
- runtime scale 상·하한, finite check, OOD gate, fallback 명시
- learned map inference의 WCET/allocator/CPU fallback 측정
- map version과 estimator Q/R version을 함께 배포·rollback

## 관련 일일 학습

- `daily_robotics/2026-10-23`: scalar covariance calibration, OOD/NIS gate, actuator supervisor 연계

## 참고

- Tsuei et al., [Learned Uncertainty Calibration for Visual Inertial Localization](https://arxiv.org/abs/2110.02136), ICRA 2021.
