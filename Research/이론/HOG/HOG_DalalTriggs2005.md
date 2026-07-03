# HOG: Histograms of Oriented Gradients 논문 해석과 직관

> 핵심 논문: **Histograms of Oriented Gradients for Human Detection**  
> 저자: Navneet Dalal, Bill Triggs  
> 발표: CVPR 2005, pp. 886-893  
> 대표 아이디어: 작은 지역마다 gradient 방향 히스토그램을 만들고, block 단위로 정규화해 물체의 형태를 표현한다.

## 한 줄 요약

HOG는 이미지를 픽셀 값 그대로 보지 않고, **밝기가 어느 방향으로 얼마나 강하게 변하는지**를 지역별 히스토그램으로 모아 물체의 윤곽과 형태를 표현하는 feature descriptor다.

사람 검출을 예로 들면, 사람의 색상이나 옷 무늬는 매번 달라도 머리, 어깨, 몸통, 다리의 **경계 방향 패턴**은 비교적 안정적이다. HOG는 이 안정적인 경계 방향 구조를 잡아낸다.

## 논문의 배경

2005년 당시 딥러닝 기반 객체 검출은 아직 주류가 아니었다. 객체 검출은 보통 다음 흐름으로 구성됐다.

```text
이미지 window 추출 -> hand-crafted feature 계산 -> classifier로 사람/비사람 분류
```

Dalal과 Triggs는 사람 검출을 실험 대상으로 삼아 어떤 feature가 가장 강건한지 분석했다. 결론은 다음과 같다.

- edge/gradient 기반 feature가 사람 형태를 잘 잡는다.
- 단순 edge 존재 여부보다 **gradient 방향 분포**가 더 강하다.
- 작은 cell의 histogram을 모으고, 주변 block 단위로 contrast normalization을 하면 조명 변화에 강해진다.
- 최종 HOG feature에 linear SVM을 붙이면 당시 기준 매우 강한 사람 검출기가 된다.

## 논문 초록의 의미 중심 번역

이 논문은 robust visual object recognition을 위한 feature set을 연구하며, linear SVM 기반 사람 검출을 테스트 사례로 사용한다. 기존 edge와 gradient descriptor를 검토한 뒤, 균일한 grid 위에서 계산한 oriented gradient histogram이 기존 feature set보다 뛰어난 성능을 낸다는 것을 실험적으로 보인다. 또한 gradient 계산, orientation binning, spatial binning, contrast normalization 등 각 단계가 성능에 어떤 영향을 주는지 분석한다.

원문 전체 번역이 아니라 핵심 의미를 학습용으로 풀어 쓴 것이다.

## HOG가 보는 것

이미지에서 픽셀 밝기가 갑자기 바뀌는 곳은 보통 경계다.

```text
밝은 영역 | 어두운 영역
```

이 경계에서는 gradient magnitude가 크고, gradient direction은 밝기가 증가하는 방향을 가리킨다. HOG는 각 픽셀의 gradient를 계산한 뒤, 방향을 몇 개 bin으로 나누어 투표한다.

예를 들어 9개 bin을 쓰면 0도부터 180도까지를 대략 20도 간격으로 나눈다.

```text
0, 20, 40, 60, 80, 100, 120, 140, 160 degrees
```

사람 윤곽은 여러 방향의 선분으로 구성되어 있으므로, 지역별 gradient 방향 히스토그램을 모으면 형태 정보가 남는다.

## HOG 처리 파이프라인

논문의 기본 처리 흐름은 다음과 같다.

```text
1. 입력 이미지 정규화
2. gradient 계산
3. cell 단위 orientation histogram 계산
4. block 단위 histogram 연결
5. block contrast normalization
6. detection window 전체 feature vector 생성
7. linear SVM으로 분류
```

### 1. Gradient 계산

가장 단순한 방식은 중심 차분이다.

```text
gx(x, y) = I(x + 1, y) - I(x - 1, y)
gy(x, y) = I(x, y + 1) - I(x, y - 1)
```

gradient 크기와 방향은 다음과 같다.

```text
magnitude = sqrt(gx^2 + gy^2)
angle = atan2(gy, gx)
```

HOG에서는 보통 unsigned orientation을 많이 쓴다. 즉, 0도와 180도를 같은 방향으로 본다.

```text
angle range: 0 <= angle < 180
```

이유는 경계의 양쪽 밝기가 바뀌어도 물체의 윤곽 방향 자체는 같다고 보기 때문이다.

### 2. Cell histogram

이미지를 작은 cell로 나눈다. 대표 설정은 `8x8 pixel cell`이다.

각 cell 안의 픽셀들은 자신의 gradient 방향 bin에 투표한다. 투표 가중치는 보통 gradient magnitude다.

```text
강한 edge -> 큰 vote
약한 edge -> 작은 vote
```

즉, HOG는 "이 cell 안에는 어느 방향의 edge가 강한가?"를 기록한다.

### 3. Block normalization

문제는 조명이다. 같은 사람이라도 밝게 찍히면 gradient magnitude가 전체적으로 커지고, 어둡게 찍히면 작아진다.

그래서 여러 cell을 묶은 block 단위로 histogram을 정규화한다. 대표 설정은 다음이다.

```text
cell: 8x8 pixels
block: 2x2 cells = 16x16 pixels
block stride: 1 cell
orientation bins: 9
normalization: L2-Hys
```

block histogram vector를 `v`라고 하면 L2 normalization은 다음과 비슷하다.

```text
v_normalized = v / sqrt(||v||_2^2 + epsilon^2)
```

L2-Hys는 여기에 clipping을 추가한다.

```text
1. L2 normalize
2. 각 값을 0.2 이하로 clip
3. 다시 L2 normalize
```

이렇게 하면 특정 edge 하나가 너무 강해서 전체 feature를 지배하는 것을 줄인다.

## 왜 잘 작동하나

### 1. 색보다 형태가 중요하다

사람 검출에서 옷 색, 배경, 조명은 계속 바뀐다. 하지만 머리-어깨-몸통-다리의 외곽선 구조는 상대적으로 안정적이다. HOG는 색상 대신 local shape를 본다.

### 2. 너무 세밀하지도, 너무 거칠지도 않다

픽셀 단위 edge는 노이즈에 약하다. 반대로 전체 이미지 하나의 histogram은 위치 정보를 잃는다. HOG는 cell과 block을 사용해 중간 정도의 spatial structure를 보존한다.

```text
local gradient + coarse spatial layout
```

이 균형이 중요하다.

### 3. 정규화가 조명 변화를 줄인다

block normalization은 HOG의 핵심이다. 논문에서도 normalization이 성능에 큰 영향을 준다고 분석한다. 같은 윤곽이라도 밝기 차이 때문에 feature scale이 달라지는 문제를 줄인다.

## 수학적으로 보는 HOG

이미지를 함수 `I(x, y)`로 보면 gradient는 다음 벡터다.

```text
∇I = [∂I/∂x, ∂I/∂y]
```

이 벡터의 크기는 edge 강도, 방향은 밝기가 증가하는 방향이다.

HOG cell histogram은 다음처럼 생각할 수 있다.

```text
h_c[k] = Σ_{p in cell c} magnitude(p) * vote(angle(p), bin k)
```

여기서 `vote()`는 angle이 해당 bin에 얼마나 가까운지를 나타낸다. 단순 구현에서는 가장 가까운 bin 하나에만 넣고, 더 정교한 구현에서는 인접 bin에 보간한다.

block vector는 여러 cell histogram을 이어 붙인 것이다.

```text
v_b = concat(h_c1, h_c2, h_c3, h_c4)
```

그리고 정규화한다.

```text
f_b = normalize(v_b)
```

마지막 HOG descriptor는 모든 block feature를 이어 붙인 벡터다.

```text
HOG(image) = concat(f_b1, f_b2, ..., f_bn)
```

## Dalal-Triggs 설정의 대표값

논문에서 사람 검출에 강하게 작동한 대표 설정은 다음과 같이 알려져 있다.

| 항목 | 대표 설정 |
|---|---|
| Detection window | 64x128 pixels |
| Cell size | 8x8 pixels |
| Block size | 2x2 cells |
| Block stride | 1 cell |
| Orientation bins | 9 |
| Orientation | unsigned, 0-180 degrees |
| Normalization | L2-Hys |
| Classifier | linear SVM |

64x128 window에서 cell은 가로 8개, 세로 16개다. block은 2x2 cell이고 stride가 1 cell이므로 block은 가로 7개, 세로 15개가 된다.

하나의 block feature 길이:

```text
2 * 2 * 9 = 36
```

전체 HOG feature 길이:

```text
7 * 15 * 36 = 3780
```

이 `3780` 차원 descriptor가 고전 HOG+SVM 사람 검출기의 대표적인 숫자다.

## SVM과의 연결

HOG는 feature extractor이고, 사람/비사람 판정은 classifier가 한다. Dalal-Triggs 논문에서는 linear SVM을 사용했다.

```text
score = w · HOG(window) + b
```

점수가 threshold보다 크면 사람, 작으면 비사람으로 판단한다.

여기서 linear SVM이 잘 맞는 이유는 HOG descriptor가 이미 형태 정보를 잘 정리해 주기 때문이다. 복잡한 classifier를 쓰기보다 좋은 feature와 단순한 classifier를 결합한 전형적인 고전 컴퓨터 비전 방식이다.

## 예제 코드

같은 폴더의 [hog_from_scratch.py](./hog_from_scratch.py)는 외부 라이브러리 없이 HOG의 핵심을 직접 구현한 학습용 코드다.

실행:

```powershell
python Research\이론\HOG\hog_from_scratch.py
```

예제는 16x16 synthetic image를 만들고 다음을 수행한다.

- 중심 차분으로 gradient 계산
- 8x8 cell의 9-bin histogram 계산
- 2x2 cell block을 L2-Hys 방식으로 정규화
- 최종 descriptor 길이와 cell histogram 출력

## 직접 해볼 학습 과제

1. `make_test_image()`에서 세로 edge를 대각선 edge로 바꿔본다.
2. `cell_size`를 4로 바꿔 feature 길이가 어떻게 변하는지 본다.
3. `bins`를 9에서 6 또는 18로 바꿔 orientation 해상도를 비교한다.
4. `normalize_l2_hys()`에서 clipping 값을 0.2에서 0.1 또는 0.4로 바꿔본다.
5. OpenCV가 설치되어 있다면 `cv2.HOGDescriptor` 결과와 직접 구현 결과의 구조를 비교해본다.

## 현재는 어떻게 발전했나

### 1. HOG + SVM에서 DPM으로

HOG는 Felzenszwalb 계열의 Deformable Part Models(DPM)에서 핵심 feature로 사용되었다. DPM은 사람이나 물체를 하나의 rigid template이 아니라 여러 part의 조합으로 보고, HOG feature 위에서 part 위치를 모델링했다.

### 2. 빠른 검출기와 cascade

HOG는 sliding window 방식이라 모든 위치와 scale을 훑으면 느리다. 이후 cascade, feature pyramid, integral histogram, fast HOG 변형들이 등장해 속도를 개선했다.

### 3. OpenCV, dlib 등 실무 라이브러리

OpenCV의 HOGDescriptor, dlib의 HOG 기반 face detector처럼 HOG는 오랫동안 실무 baseline으로 쓰였다. 딥러닝 이전 시대에는 사람 검출, 보행자 검출, 간단한 객체 검출에서 매우 중요한 도구였다.

### 4. CNN으로의 전환

AlexNet 이후 객체 인식과 검출은 CNN feature가 주도하게 되었다. CNN의 초기 convolution filter는 edge와 orientation에 반응한다는 점에서 HOG와 닮은 면이 있다. 차이는 HOG가 사람이 설계한 feature이고, CNN은 데이터를 통해 feature를 학습한다는 점이다.

### 5. 지금도 유용한 이유

HOG는 최신 SOTA는 아니지만 여전히 가치가 있다.

- 데이터가 적은 과제에서 강한 baseline이 된다.
- feature engineering과 gradient 기반 표현을 배우기에 좋다.
- 임베디드/저사양 환경에서 가벼운 detector를 만들 때 쓸 수 있다.
- 딥러닝 feature가 무엇을 학습하는지 이해하는 다리 역할을 한다.

## HOG를 한 문장으로 다시 정리

HOG는 이미지를 "픽셀 밝기 배열"이 아니라 **지역별 edge 방향 분포 지도**로 바꾸는 방법이다. 그리고 이 지도는 사람이나 물체의 윤곽 구조를 조명 변화에 비교적 강하게 표현한다.

## 참고 자료

- Dalal and Triggs, IEEE Xplore: https://ieeexplore.ieee.org/document/1467360
- Dalal and Triggs PDF mirror: https://www.cs.princeton.edu/courses/archive/fall13/cos429/papers/Dalal05.pdf
- DBLP entry: https://dblp.org/rec/conf/cvpr/DalalT05
- Semantic Scholar entry: https://www.semanticscholar.org/paper/Histograms-of-oriented-gradients-for-human-Dalal-Triggs/e8b12467bdc20bde976750b8a28decdb33246d1d
