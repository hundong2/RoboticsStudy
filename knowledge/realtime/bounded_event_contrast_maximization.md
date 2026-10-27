# Bounded Event Contrast Maximization

## 핵심 파이프라인

1. 최대 `N`개의 event packet을 fixed-capacity queue로 받는다.
2. callback/wakeup당 최대 packet 수를 제한한다.
3. 최대 `K`개의 motion hypotheses를 평가한다.
4. 각 hypothesis로 event를 기준 시각에 warp한다.
5. fixed-size Image of Warped Events histogram의 contrast를 계산한다.
6. best score뿐 아니라 peak separation, 유효 event 수, drop, 실행시간을 publish한다.

## 수식과 작업량

등속 2D flow의 warp는 다음과 같다.

\[
x'_k=x_k+v_x(t_{ref}-t_k),\qquad
y'_k=y_k+v_y(t_{ref}-t_k)
\]

`B`개 histogram bin을 매 hypothesis마다 초기화하고 훑는 단순 구현의 작업량은 대략 다음처럼 제한된다.

\[
O\left(K(B+N)\right)
\]

상한이 분명하다는 장점이 있지만, 큰 sensor에서는 `B` 초기화가 지배적일 수 있다. active-bin list, generation counter, tile histogram, SIMD를 쓰면 평균 작업을 줄일 수 있으나 각 최적화가 새로운 상태와 검증 부담을 만든다.

## RT 설계 규칙

- subscription callback에서 optimization을 하지 않고 ownership만 bounded queue로 넘긴다.
- queue가 차면 blocking하지 말고 drop/coalesce 정책과 counter를 명시한다.
- worker가 한 wake-up에 queue 전체를 비우지 않게 한다.
- grid/iteration/event/window 크기는 parameter validation으로 상한을 강제한다.
- `max_kernel_us`는 관측된 high-water mark이지 WCET가 아님을 표시한다.
- executor callback group 격리와 OS real-time scheduling을 혼동하지 않는다.

## 관측성 telemetry

best hypothesis만 publish하면 texture가 없어도 숫자는 나온다. 최소한 다음을 함께 본다.

- best/second 또는 non-neighbor peak ratio
- local Hessian/curvature eigenvalues
- event의 image-plane spatial coverage
- polarity balance
- 유효 warp 비율과 image boundary loss
- time-span과 event count

한 방향 edge는 edge 접선 방향 속도를 구분하기 어려운 aperture problem을 만든다. peak가 뾰족하다는 사실만으로 2D 속도 전체가 관측 가능하다고 결론 내리지 않는다.

## 실패 모드

- event collapse: 잘못된 warp가 event를 비물리적으로 한 곳에 모음
- dynamic mixture: 여러 object 속도가 하나의 objective에 섞임
- timestamp bias/drift: 모든 warp가 일관되게 틀어짐
- background activity/hot pixels: 움직임과 무관한 peak 생성
- coarse grid quantization: 정답이 후보 사이에 있을 때 bias와 계단형 출력

## 참고

- [Gallego et al., CVPR 2018](https://openaccess.thecvf.com/content_cvpr_2018/html/Gallego_A_Unifying_Contrast_CVPR_2018_paper.html)
- [DOI: 10.1109/CVPR.2018.00407](https://doi.org/10.1109/CVPR.2018.00407)
