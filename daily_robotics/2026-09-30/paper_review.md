# 논문 리뷰 — CoCo-InEKF

## 서지 정보

- Michael Baumgartner et al., **“CoCo-InEKF: State Estimation with Learned Contact Covariances in Dynamic, Contact-Rich Scenarios”**
- RSS 2026, arXiv:2605.15122v1, 2026-05-14
- [초록/PDF](https://arxiv.org/abs/2605.15122) · [HTML 본문](https://arxiv.org/html/2605.15122)

## 1. 해결하려는 문제

보행 로봇의 상태 추정기는 “발이 땅에 닿아 있으면 그 접촉점은 월드에서 정지한다”는 제약을 자주 사용한다. 이 가정은 단단한 평지에서는 강력하지만 춤, 발 모서리 접촉, 구르기, 방향성 미끄럼처럼 접촉이 애매한 순간에 깨진다.

기존의 binary contact는 이 상황을 `0/1` 중 하나로 강제로 고른다. `1`을 고르면 미끄러지는 발을 정지 랜드마크로 믿어 state가 끌려가고, `0`을 고르면 아직 유용한 부분 접촉 정보까지 버린다. threshold 주변에서는 작은 센서 잡음이 접촉점의 state 추가/삭제를 반복시켜 계산 그래프도 불연속이 된다.

## 2. 핵심 아이디어와 수학적 직관

### 접촉 상태가 아니라 접촉 속도 covariance를 예측한다

각 contact candidate의 월드 위치 `p_Ci`를 항상 state에 유지하고, 접촉점 속도를 다음처럼 둔다.

```text
d(p_Ci)/dt = -R_WB w_Ci
w_Ci ~ N(0, Sigma_Ci)
```

`Sigma_Ci`가 작으면 “이 점은 거의 정지했다”는 강한 제약이고, 크면 “움직일 수 있으니 덜 믿어라”라는 뜻이다. 특히 3×3 covariance는 방향별로 다르게 만들 수 있어, 법선 방향은 단단하지만 접선 방향은 미끄러지는 접촉도 표현한다.

논문은 경량 신경망이 각 후보점의 `Sigma_Ci`를 예측하게 한다. covariance가 대칭 positive-semidefinite가 되도록 lower-triangular `L`의 6개 원소를 예측하고 `Sigma=L L^T`로 구성한다. 이 형태는 음의 분산 같은 물리적으로 불가능한 출력을 막는다.

### 필터를 differentiable하게 만든다

접촉점을 state에 넣었다 뺐다 하지 않고 모두 유지하면 계산 그래프가 고정된다. 그러면 InEKF의 prediction/correction을 통과해 최종 state error까지 backpropagation할 수 있다. 별도의 “접촉 정답 라벨”을 사람이 heuristic으로 만들지 않고도, trajectory state error로 contact covariance module을 end-to-end 학습할 수 있다.

### 물리 필터와 학습의 역할 분담

IMU 적분, Lie-group state, kinematic measurement, Kalman covariance update는 해석 가능한 필터가 담당한다. 신경망은 가장 모델링하기 어려운 “지금 이 접촉 제약을 어느 방향으로 얼마나 믿을까?”만 담당한다. 순수 end-to-end state regressor보다 구조적 prior를 보존하는 hybrid 접근이다.

## 3. 실무 적용 가능성

- 접촉이 풍부한 humanoid/legged motion에서 force threshold 하나보다 자연스러운 confidence를 downstream estimator/controller에 전달할 수 있다.
- directional covariance는 foot roll, edge contact, 한 방향 slip을 scalar probability보다 잘 표현한다.
- 모든 contact candidate를 유지하는 고정 계산 구조는 학습뿐 아니라 배포 시 shape 안정성과 profiling에도 유리하다.
- covariance를 estimator 내부에서 끝내지 않고 safety supervisor에 전달하면, 오늘 실습처럼 명령 한계와 fallback 기준을 불확실성에 맞춰 조정할 수 있다.

## 한계와 엔지니어링 질문

1. **학습 분포 밖 동작:** 춤/바닥/신발/충돌이 학습 데이터와 달라지면 covariance가 과신될 수 있다. covariance calibration과 OOD 감지가 별도로 필요하다.
2. **평균 정확도와 hard RT는 다르다:** 경량 네트워크와 필터의 평균 처리량이 좋아도 deadline/WCET를 보장하지 않는다. inference backend, 메모리 할당, accelerator contention을 측정해야 한다.
3. **covariance는 안전 인증서가 아니다:** 작게 예측된 covariance가 실제 접촉을 보장하지 않는다. 독립 force/slip guard와 actuator-side torque/rate limit가 필요하다.
4. **시간 동기/외부 파라미터:** IMU, encoder, contact frame의 stamp와 calibration 오류는 contact model이 흡수해 버릴 수 있다. 입력 계약을 먼저 검증해야 한다.
5. **후보점 수와 계산량:** 후보점을 항상 state에 유지하므로 수가 늘면 filter dimension과 update 비용이 증가한다. 자동 후보 선택 결과도 robot morphology 변경 때 재검증해야 한다.

## 오늘 코드와 논문의 경계

오늘의 `contact_covariance_estimator`는 InEKF나 신경망을 구현하지 않는다. `F_n`, `F_t`, `v_slip`으로 contact probability와 scalar velocity variance를 만들고, 고정 상태 EWMA로 `mu_lower`를 추정한다. 학습 목표는 다음 세 가지다.

- binary contact 대신 연속 uncertainty를 메시지 계약으로 노출하기
- uncertainty를 `mu_lower F_n - |F_t|`라는 제어 가능한 물리 여유로 연결하기
- 추정기와 별도 노드가 debounce/hold/recovery, stale data, 출력 clamp를 감사하기

따라서 이 코드는 논문의 재현 결과나 성능 비교로 인용하면 안 된다. 실제 재현은 저자 구현, 동일 dataset, InEKF state/covariance, 학습 loss와 평가 metric이 필요하다.
