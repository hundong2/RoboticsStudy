# 논문 리뷰 — Event Camera Contrast Maximization

## 논문 정보와 선정 이유

- Guillermo Gallego, Henri Rebecq, Davide Scaramuzza
- **A Unifying Contrast Maximization Framework for Event Cameras, With Applications to Motion, Depth, and Optical Flow Estimation**
- CVPR 2018, pp. 3867–3876
- [CVF Open Access](https://openaccess.thecvf.com/content_cvpr_2018/html/Gallego_A_Unifying_Contrast_CVPR_2018_paper.html) · [PDF](https://openaccess.thecvf.com/content_cvpr_2018/papers/Gallego_A_Unifying_Contrast_CVPR_2018_paper.pdf) · [DOI](https://doi.org/10.1109/CVPR.2018.00407)

event camera는 frame을 주기적으로 내보내지 않는다. 밝기 변화가 생긴 pixel만 `(x,y,t,polarity)` event를 비동기로 보낸다. 이 논문은 motion, depth, optical flow처럼 서로 달라 보이는 문제를 **후보 모델로 event를 warp한 뒤 가장 잘 정렬되는 모델을 찾는다**는 하나의 관점으로 묶었다. 오늘의 2D bounded grid 구현이 이 핵심을 가장 작은 형태로 직접 보여 주기 때문에 선정했다.

## 1. 해결하려는 문제

일반 camera에서는 한 frame 안의 pixel들이 같은 시각의 image를 이룬다고 가정한다. event camera에서는 각 event가 서로 다른 microsecond 시각을 가진다. 이들을 단순히 일정 구간 동안 누적하면 움직이는 edge가 길게 번진다. 즉, event의 장점인 높은 시간 해상도를 버리고 motion blur와 비슷한 현상을 다시 만든다.

문제는 다음과 같다.

> event를 어떤 trajectory로 옮겨야 동일한 물리 edge에서 발생한 event들이 한 위치에 정렬되는가?

명시적인 feature correspondence를 먼저 만들려 하면 sparse/noisy event stream에서 데이터 연관 자체가 어려워진다. 논문은 데이터 연관을 별도 단계로 풀기보다, event 전체가 만들어내는 정렬 품질을 목적함수로 사용한다.

## 2. 핵심 아이디어와 수학적 직관

### 2.1 후보 trajectory로 기준 시각에 warp한다

event를 `e_k=(x_k,t_k,p_k)`라고 하고, 문제별 모델 parameter를 `θ`라고 하자. warp `W`는 event를 공통 기준 시각으로 옮긴다.

\[
x'_k=W(x_k,t_k;\theta)
\]

오늘의 patch optical flow처럼 등속 2D translation만 가정하면 `θ=(v_x,v_y)`이고 다음과 같이 단순해진다.

\[
x'_k=x_k+v_x(t_{ref}-t_k),\qquad
y'_k=y_k+v_y(t_{ref}-t_k)
\]

후보 속도가 실제 속도와 같으면 같은 edge가 만든 event들이 기준 시각의 같은 위치로 돌아온다. 틀린 속도라면 event가 여전히 선이나 띠처럼 퍼진다.

### 2.2 Image of Warped Events를 만든다

warp된 event를 image plane에 누적한 것이 Image of Warped Events(IWE)다.

\[
H(u;\theta)=\sum_k b_k\,\delta(u-x'_k(\theta))
\]

`b_k`는 polarity 또는 단순 count가 될 수 있다. 오늘 코드는 양/음 polarity를 별도 histogram에 넣는다. 반대 polarity가 같은 bin에서 상쇄되어 실제 정렬 peak가 약해지는 일을 피하기 위해서다.

### 2.3 “선명함”을 contrast로 측정한다

정렬된 IWE는 소수 pixel에 event가 높게 쌓이고 나머지는 비어 있어 분산이 크다. 퍼진 IWE는 여러 pixel에 고르게 분산되어 분산이 작다.

\[
f(\theta)=\operatorname{Var}(H(\cdot;\theta)),\qquad
\theta^*=\arg\max_\theta f(\theta)
\]

이 관점의 장점은 “event A가 event B와 같은 feature인가?”를 일일이 결정하지 않아도 된다는 것이다. 올바른 geometry가 전체 event cloud를 스스로 정렬하게 만든다.

### 2.4 문제에 따라 warp만 바뀐다

논문의 통합성은 목적함수보다 warp 설계에서 나온다.

- optical flow: patch 안의 등속 2D translation
- rotational motion: camera angular velocity가 만드는 spherical/image trajectory
- homography: planar scene/camera motion이 만드는 projective warp
- depth: 알려진 camera pose trajectory에서 후보 depth마다 달라지는 image trajectory

즉, **warp는 물리/geometry 모델이고 contrast는 모델이 event를 얼마나 잘 설명하는지 재는 공통 score**다.

## 3. 실무 적용 가능성

### 고속·HDR motion estimation

event camera는 빠른 회전, 밝고 어두운 영역이 함께 있는 장면, frame camera가 blur되는 상황에서 유리하다. contrast maximization으로 motion-corrected edge image를 만들면 후단 tracking, mapping, visual-inertial odometry의 입력으로 사용할 수 있다.

### feature correspondence 없는 direct frontend

descriptor matching이 불안정한 event stream에서 직접 목적함수를 최적화할 수 있다. 다만 “feature가 필요 없다”가 “scene structure나 observability가 필요 없다”는 뜻은 아니다. 한 방향 edge만 있으면 aperture problem이 남는다.

### RT 제품 구조에 넣는 방법

논문의 연속 optimizer를 그대로 callback에 넣으면 iteration 수와 latency가 데이터에 따라 달라질 수 있다. 제품에서는 다음과 같은 경계가 필요하다.

1. 입력 event 수와 time window 상한
2. optimizer iteration 또는 hypothesis 수 상한
3. callback마다 처리할 packet 수 상한
4. contrast peak curvature/ratio와 spatial coverage telemetry
5. overload 시 명시적인 drop/coalesce policy

오늘 코드는 정확히 49 hypotheses를 모두 평가해 품질과 worst-work를 단순하게 만든다. 이는 정밀한 제품 optimizer가 아니라 bounded 설계를 배우기 위한 선택이다.

## 4. 한계와 제품 적용 시 주의점

### 지역 최적점과 초기화

연속 contrast objective는 비볼록일 수 있다. gradient/Newton 계열 optimizer는 초기값에 따라 다른 peak로 갈 수 있다. coarse-to-fine, IMU prior, multi-start가 도움이 되지만 모두 계산량과 failure mode를 추가한다.

### event collapse와 잘못된 sharpness

높은 contrast가 항상 올바른 motion을 뜻하지는 않는다. 잘못된 warp가 event를 인위적으로 작은 영역에 몰아넣어 contrast를 키우는 event collapse가 생길 수 있다. 유효 image 영역, warp Jacobian, regularization, scene/model consistency를 함께 검사해야 한다.

### 모델 불일치

오늘의 등속 2D flow는 patch 하나가 같은 속도로 움직인다고 가정한다. 실제 장면의 depth 차이, 회전, moving object, acceleration이 크면 단일 velocity로 모든 event를 정렬할 수 없다. 이때 peak ratio 저하는 유용한 신호지만 원인을 자동으로 구분해 주지는 않는다.

### sensor 비이상성

background activity, hot pixel, threshold mismatch, refractory period, timestamp jitter는 IWE에 structured noise를 만든다. 단순 count histogram은 이를 모두 같은 event로 믿는다. 실제 driver 앞단의 calibration/filtering과 polarity/temporal consistency gate가 필요하다.

### contrast와 uncertainty는 다르다

best/second ratio가 높아도 metric uncertainty가 올바르게 보정되었다고 볼 수 없다. grid 해상도, time window, event count, edge 방향에 따라 속도 uncertainty가 달라진다. 후단 Kalman/filter graph에 넣으려면 Hessian, bootstrap, empirical calibration 등으로 covariance를 별도 검증해야 한다.

### 논문과 오늘 구현의 경계

논문은 여러 geometry와 optimization 방법을 포괄하는 framework다. 오늘 구현은 다음과 같이 의도적으로 축소했다.

- 64×48 synthetic event packet
- 128 fixed events
- global constant 2D optical flow
- 100 px/s 간격의 7×7 exhaustive grid
- nearest-pixel histogram과 variance score

따라서 논문의 depth, homography, rotation, continuous optimization 정확도를 재현했다고 주장할 수 없다. 대신 ROS 2 timestamp contract, bounded execution, observability telemetry를 하나의 컴파일 가능한 예제로 연결했다.

## 엔지니어 관점의 결론

이 논문의 가장 중요한 메시지는 “event를 frame으로 바꾸는 법”이 아니라 **시간을 geometry 안에 끝까지 남겨 두는 법**이다. 올바른 motion model은 event를 기준 시각에 다시 모으고, 잘못된 model은 퍼뜨린다. 제품에 적용할 때는 이 우아한 목적함수 위에 입력 상한, optimizer budget, peak 관측성, sensor fault, 모델 불일치 감시를 반드시 더해야 한다.
