# Blob

## Blob이란?

Blob은 OpenCV DNN, Caffe, ONNX Runtime 같은 딥러닝 추론 코드에서 모델에 넣기 좋게 정리한 다차원 입력 텐서입니다. 일반 이미지가 보통 `H x W x C` 형태라면, DNN blob은 보통 `N x C x H x W` 형태를 사용합니다.

- `N`: batch 크기입니다. 한 번에 몇 장의 이미지를 넣는지 나타냅니다.
- `C`: channel 수입니다. RGB/BGR 이미지는 보통 3입니다.
- `H`: height입니다.
- `W`: width입니다.

예를 들어 `1 x 3 x 224 x 224` blob은 224x224 이미지 1장을 3채널 입력으로 넣는다는 뜻입니다.

OpenCV에서는 다음 함수로 이미지에서 blob을 만들 수 있습니다.

```python
blob = cv2.dnn.blobFromImage(
    image,
    scalefactor=1.0,
    size=(224, 224),
    mean=(104, 117, 123),
    swapRB=False,
    crop=False,
)
```

이 함수는 내부적으로 크기 조절, 평균 차감, 스케일링, 채널 순서 변경, 차원 순서 변경을 수행합니다.

## 왜 Blob 전처리가 필요한가?

딥러닝 모델은 학습할 때 사용한 입력 형식이 있습니다. 추론할 때도 같은 형식으로 맞춰야 모델이 정상적으로 동작합니다. 입력 형식이 달라지면 같은 이미지라도 모델이 전혀 다른 값으로 해석할 수 있습니다.

대표적으로 확인해야 하는 항목은 다음과 같습니다.

- 입력 크기: `224x224`, `300x300`, `416x416`, `640x640` 등
- 색상 순서: OpenCV의 `BGR` 또는 일반 딥러닝 코드의 `RGB`
- 값 범위: `0~255`, `0~1`, `-1~1`
- 평균 차감: ImageNet mean, Caffe mean 등
- 표준편차 정규화: PyTorch 계열에서 자주 사용
- 차원 순서: `NHWC` 또는 `NCHW`

## OpenCV `blobFromImage` 처리 순서

OpenCV의 `blobFromImage`는 개념적으로 다음 계산을 수행합니다.

```text
blob = scalefactor * (resized_image - mean)
```

그 뒤 결과를 모델 입력에 맞게 `N x C x H x W` 형태로 바꿉니다.

주의할 점은 `mean`을 먼저 빼고 그 결과에 `scalefactor`가 곱해진다는 것입니다. 즉 `scalefactor=1/255.0`이면 평균값도 같은 비율로 함께 스케일됩니다.

## 평균 차감법(Mean Subtraction)

평균 차감법은 이미지 픽셀에서 채널별 평균값을 빼는 전처리입니다.

```text
R' = R - mean_R
G' = G - mean_G
B' = B - mean_B
```

이 처리를 하는 이유는 입력 분포를 학습 당시 분포와 맞추기 위해서입니다. 모델은 특정 평균과 분산을 가진 데이터로 학습되었기 때문에, 추론 입력도 비슷한 분포로 맞춰 주는 것이 중요합니다.

### Caffe 계열 평균값

Caffe로 학습된 오래된 ImageNet 모델은 BGR 순서 평균값을 쓰는 경우가 많습니다.

```python
mean=(104, 117, 123)  # B, G, R 순서
swapRB=False
```

OpenCV는 이미지를 기본적으로 BGR로 읽기 때문에, Caffe 모델을 사용할 때는 `swapRB=False`가 자연스럽습니다.

### PyTorch/ImageNet 계열 평균값

PyTorch torchvision 모델은 보통 RGB 순서에서 `0~1`로 스케일한 뒤 mean/std 정규화를 합니다.

```python
mean=(0.485, 0.456, 0.406)  # R, G, B
std=(0.229, 0.224, 0.225)
```

OpenCV `blobFromImage`는 표준편차 나누기를 직접 인자로 받지 않습니다. 이 경우에는 수동으로 처리하거나, `blobFromImages` 이후 채널별로 나눠야 합니다.

```python
image_rgb = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2RGB)
image_float = image_rgb.astype(np.float32) / 255.0
image_float = (image_float - mean) / std
blob = image_float.transpose(2, 0, 1)[None, ...]
```

### 평균 차감을 잘못하면 생기는 문제

- 전체 confidence가 낮아집니다.
- 엉뚱한 class가 높은 점수를 받을 수 있습니다.
- 객체 검출 박스는 나오지만 class가 불안정할 수 있습니다.
- 밝기/색상 변화에 과하게 민감해질 수 있습니다.

## 크기 조절(Resize)

모델은 고정된 입력 크기를 기대하는 경우가 많습니다. 예를 들어 AlexNet/ResNet은 `224x224`, SSD는 `300x300`, YOLO 계열은 `416x416`, `640x640` 같은 크기를 자주 사용합니다.

OpenCV에서는 `size=(width, height)` 순서로 지정합니다.

```python
blob = cv2.dnn.blobFromImage(image, size=(300, 300))
```

여기서 `size=(300, 300)`은 `width=300`, `height=300`입니다. NumPy 이미지 shape가 `(height, width, channels)`인 것과 순서가 반대라 헷갈리기 쉽습니다.

### 단순 resize

단순 resize는 원본 이미지를 모델 입력 크기로 바로 늘리거나 줄입니다.

장점:

- 구현이 단순합니다.
- 대부분의 예제 코드에서 바로 사용할 수 있습니다.
- 분류 모델이나 일부 검출 모델에서 충분히 잘 동작합니다.

단점:

- 원본 비율이 바뀌어 객체가 찌그러질 수 있습니다.
- 객체 검출 박스를 원본 좌표로 복원할 때 주의해야 합니다.

### 비율 유지 resize와 padding(letterbox)

YOLO 계열에서는 원본 비율을 유지하고 남는 영역을 padding으로 채우는 letterbox 방식이 자주 쓰입니다.

예를 들어 `1280x720` 이미지를 `640x640`에 넣는다면 이미지를 `640x360`으로 줄이고, 위아래에 padding을 추가합니다.

장점:

- 객체 비율이 보존됩니다.
- 검출 모델에서 위치 예측이 더 안정적일 수 있습니다.

단점:

- padding 영역을 고려해서 박스 좌표를 원본으로 되돌려야 합니다.
- 단순 `blobFromImage`만으로는 letterbox 후처리까지 자동 관리되지 않습니다.

### crop 옵션

`blobFromImage(..., crop=True)`를 사용하면 이미지를 resize한 뒤 중앙을 잘라 입력 크기에 맞춥니다.

```python
blob = cv2.dnn.blobFromImage(image, size=(224, 224), crop=True)
```

분류 모델에서는 center crop이 학습/검증 과정과 맞을 수 있습니다. 하지만 객체 검출에서는 이미지 가장자리 객체가 잘릴 수 있으므로 보통 `crop=False`가 더 안전합니다.

## 스케일링(Scaling)

스케일링은 픽셀 값 범위를 바꾸는 과정입니다.

OpenCV로 읽은 이미지는 보통 `uint8`이고 값 범위가 `0~255`입니다. 많은 딥러닝 모델은 `float32` 입력을 기대합니다.

### `0~255` 유지

Caffe 계열 모델은 평균만 빼고 `0~255` 스케일을 유지하는 경우가 있습니다.

```python
blob = cv2.dnn.blobFromImage(
    image,
    scalefactor=1.0,
    mean=(104, 117, 123),
)
```

### `0~1` 변환

픽셀 값을 `0~1`로 바꿀 때는 `1/255.0`을 곱합니다.

```python
blob = cv2.dnn.blobFromImage(
    image,
    scalefactor=1.0 / 255.0,
    mean=(0, 0, 0),
)
```

### `-1~1` 변환

일부 모바일/경량 모델은 입력 범위를 `-1~1`로 맞춥니다.

```text
x' = x / 127.5 - 1.0
```

OpenCV에서는 아래처럼 표현할 수 있습니다.

```python
blob = cv2.dnn.blobFromImage(
    image,
    scalefactor=1.0 / 127.5,
    mean=(127.5, 127.5, 127.5),
)
```

계산 순서가 `scalefactor * (image - mean)`이므로 결과는 `(image - 127.5) / 127.5`가 됩니다.

## 채널 교체(Channel Swap)

OpenCV의 `cv2.imread()`는 이미지를 `BGR` 순서로 읽습니다. 하지만 PyTorch, TensorFlow, PIL, 대부분의 문서 예제는 `RGB` 순서를 기준으로 설명합니다.

`blobFromImage`의 `swapRB=True`는 첫 번째 채널과 세 번째 채널을 바꿉니다.

```python
blob = cv2.dnn.blobFromImage(
    image_bgr,
    scalefactor=1.0 / 255.0,
    size=(224, 224),
    mean=(0, 0, 0),
    swapRB=True,
)
```

### 언제 `swapRB=True`를 쓰나?

- 모델이 RGB 이미지로 학습된 경우
- PyTorch/torchvision 모델을 ONNX로 export한 경우
- TensorFlow 모델이 RGB 입력을 기대하는 경우
- Ultralytics YOLO 계열처럼 일반적으로 RGB 전처리를 기준으로 하는 경우

### 언제 `swapRB=False`를 쓰나?

- Caffe 모델이 BGR 평균값을 기준으로 학습된 경우
- OpenCV DNN 예제에서 Caffe 모델을 그대로 사용하는 경우
- 모델 문서가 BGR 입력을 명시한 경우

채널 순서가 틀리면 빨간색과 파란색 정보가 뒤바뀝니다. 사람 눈에는 비슷한 장면처럼 보일 수 있지만 모델 입장에서는 완전히 다른 입력입니다.

## 차원 순서 변경: HWC에서 NCHW로

OpenCV 이미지는 일반적으로 다음 형태입니다.

```text
H x W x C
```

딥러닝 모델은 다음 형태를 많이 씁니다.

```text
N x C x H x W
```

직접 변환하면 다음과 같습니다.

```python
image = cv2.resize(image, (224, 224))
image = image.astype(np.float32)
image = image.transpose(2, 0, 1)  # HWC -> CHW
blob = image[None, ...]           # CHW -> NCHW
```

`blobFromImage`는 이 변환을 자동으로 수행합니다.

## `blobFromImage` 주요 인자

| 인자 | 의미 | 자주 쓰는 값 |
| --- | --- | --- |
| `image` | 입력 이미지 | `cv2.imread()` 결과 |
| `scalefactor` | 평균 차감 뒤 곱할 값 | `1.0`, `1/255.0`, `1/127.5` |
| `size` | 모델 입력 크기 | `(224,224)`, `(300,300)`, `(640,640)` |
| `mean` | 채널별 평균값 | `(104,117,123)`, `(0,0,0)`, `(127.5,127.5,127.5)` |
| `swapRB` | BGR/RGB 교체 여부 | Caffe는 대개 `False`, PyTorch/YOLO는 대개 `True` |
| `crop` | resize 후 중앙 crop 여부 | 분류는 상황에 따라 `True`, 검출은 보통 `False` |
| `ddepth` | blob 데이터 타입 | 보통 `cv2.CV_32F` |

## 모델별 전처리 예시

### Caffe SSD MobileNet

```python
blob = cv2.dnn.blobFromImage(
    image,
    scalefactor=0.007843,
    size=(300, 300),
    mean=(127.5, 127.5, 127.5),
    swapRB=False,
    crop=False,
)
```

이 설정은 `(image - 127.5) * 0.007843`에 가깝습니다. 즉 대략 `-1~1` 범위로 맞춥니다.

### Caffe ImageNet 모델

```python
blob = cv2.dnn.blobFromImage(
    image,
    scalefactor=1.0,
    size=(224, 224),
    mean=(104, 117, 123),
    swapRB=False,
    crop=True,
)
```

Caffe ImageNet 모델은 BGR 평균값을 사용하는 경우가 많습니다.

### YOLO 계열

```python
blob = cv2.dnn.blobFromImage(
    image,
    scalefactor=1.0 / 255.0,
    size=(640, 640),
    mean=(0, 0, 0),
    swapRB=True,
    crop=False,
)
```

YOLO는 구현체마다 letterbox가 필요할 수 있습니다. 단순 resize로 학습된 모델인지, letterbox 전처리를 기준으로 학습/배포되는 모델인지 확인해야 합니다.

### PyTorch/ONNX 분류 모델

PyTorch 모델은 mean/std 정규화가 필요할 수 있으므로 수동 처리하는 편이 명확합니다.

```python
image_rgb = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2RGB)
image_rgb = cv2.resize(image_rgb, (224, 224))
image_float = image_rgb.astype(np.float32) / 255.0

mean = np.array([0.485, 0.456, 0.406], dtype=np.float32)
std = np.array([0.229, 0.224, 0.225], dtype=np.float32)
image_float = (image_float - mean) / std

blob = image_float.transpose(2, 0, 1)[None, ...]
```

## Blob 확인 방법

전처리가 맞는지 확인하려면 blob shape와 값 범위를 출력해 보는 것이 좋습니다.

```python
print(blob.shape)
print(blob.dtype)
print(blob.min(), blob.max(), blob.mean())
```

예상 예시는 다음과 같습니다.

```text
(1, 3, 224, 224)
float32
-2.1 2.6 0.12
```

값 범위가 예상과 크게 다르면 `scalefactor`, `mean`, `swapRB`를 다시 확인해야 합니다.

## 자주 하는 실수

- `size=(height, width)`로 착각합니다. OpenCV DNN은 `(width, height)` 순서입니다.
- Caffe 평균값 `(104,117,123)`을 RGB 모델에 그대로 씁니다.
- `swapRB=True`를 켜야 하는 모델에서 끄거나, 반대로 Caffe BGR 모델에서 켭니다.
- `0~1` 모델에 `0~255` 값을 넣습니다.
- PyTorch 모델의 `std` 나누기를 생략합니다.
- 객체 검출에서 letterbox padding을 했는데 박스 좌표 복원 시 padding을 빼지 않습니다.
- 모델 output에 NMS가 포함되어 있는지 모르고 NMS를 두 번 적용합니다.

## 핵심 정리

Blob은 단순한 이미지 배열이 아니라 모델이 학습 때 보던 입력 형식으로 맞춘 텐서입니다. 평균 차감, 크기 조절, 스케일링, 채널 교체, 차원 변환 중 하나만 틀려도 추론 품질이 크게 떨어질 수 있습니다.

실무에서는 모델을 받을 때 다음 정보를 반드시 같이 확인해야 합니다.

- 입력 tensor 이름과 shape
- RGB/BGR 순서
- resize 방식과 crop/letterbox 여부
- pixel scale 범위
- mean/std 값
- batch 차원 포함 여부
- 후처리 방식
