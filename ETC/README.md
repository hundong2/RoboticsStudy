# 딥러닝 비전 용어 정리

## Getting Start

이 폴더의 실습 코드는 `uv`로 실행할 수 있습니다.

```powershell
cd D:\workspace\RoboticsStudy\ETC
uv sync
uv run python play\dnn_forward_inference_demo.py
uv run python play\simd_vectorization_demo.py
uv run python play\onnx_runtime_demo.py
uv run python play\object_detection_boxes_demo.py
uv run python play\opencv_dnn_info.py
```

`uv`가 없다면 먼저 설치합니다.

```powershell
winget install astral-sh.uv
```

## 목차

- [DNN](#dnn) - [실습](play/dnn_forward_inference_demo.py)
- [SSE(Streaming SIMD Extensions)](#ssestreaming-simd-extensions) - [실습](play/simd_vectorization_demo.py)
- [AVX(Intel Advanced Vector Extensions)](#avxintel-advanced-vector-extensions) - [실습](play/simd_vectorization_demo.py)
- [AVX2](#avx2) - [실습](play/simd_vectorization_demo.py)
- [NEON](#neon) - [실습](play/simd_vectorization_demo.py)
- [CUDA](#cuda)
- [forward(순전파)](#forward순전파) - [실습](play/dnn_forward_inference_demo.py)
- [inference(추론)](#inference추론) - [실습](play/dnn_forward_inference_demo.py)
- [Blob 전처리](tech/Blob.md)
- [Deeplearning framework](#deeplearning-framework)
  - [모델/프레임워크 설정 파일](#모델프레임워크-설정-파일)
  - [ONNX](#onnx) - [실습](play/onnx_runtime_demo.py)
  - [Caffe](#caffe)
  - [Darknet](#darknet)
  - [TensorFlow](#tensorflow)
- [Deeplearning model](#deeplearning-model)
  - [AlexNet](#alexnet)
  - [GoogLeNet](#googlenet)
  - [ResNet](#resnet)
  - [SSD](#ssd) - [실습](play/object_detection_boxes_demo.py)
  - [YOLO](#yolo) - [실습](play/object_detection_boxes_demo.py)
  - [Faster R-CNN](#faster-r-cnn) - [실습](play/object_detection_boxes_demo.py)
  - [OpenVINO](#openvino) - [실습](play/opencv_dnn_info.py)

## DNN

### 용어 및 기초 지식

DNN(Deep Neural Network)은 입력층, 여러 개의 은닉층, 출력층으로 구성된 신경망입니다. 이미지 분류, 객체 검출, 음성 인식처럼 입력과 출력 사이의 규칙을 사람이 직접 코딩하기 어려운 문제에서 데이터를 통해 특징과 판단 기준을 학습합니다.

### 역사

초기 신경망 연구는 퍼셉트론과 다층 퍼셉트론에서 시작했고, 2000년대 후반 GPU 학습과 대규모 데이터셋이 결합되며 다시 급성장했습니다. 2012년 AlexNet이 ImageNet 대회에서 큰 성능 향상을 보이면서 컴퓨터 비전의 주류 접근법이 되었습니다.

### 사용하는 곳

로봇 비전, 자율주행, 제조 결함 검사, 의료 영상, OCR, 얼굴 인식, 추천 시스템, 자연어 처리에 사용됩니다.

### 최신 트렌드

2026년 기준 비전 분야는 CNN 단독 모델보다 Transformer, foundation model, vision-language model, edge AI 최적화가 함께 쓰이는 흐름입니다. 로봇에서는 카메라 입력을 언어/행동 계획과 연결하는 멀티모달 모델의 중요성이 커지고 있습니다.

### 참고 자료

- [Deep Learning, Nature 2015](https://www.nature.com/articles/nature14539)
- [ImageNet Classification with Deep Convolutional Neural Networks](https://papers.nips.cc/paper/4824-imagenet-classification-with-deep-convolutional-neural-networks)
- [PyTorch Tutorials](https://docs.pytorch.org/tutorials/)

## SSE(Streaming SIMD Extensions)

### 용어 및 기초 지식

SSE는 Intel x86 계열 CPU의 SIMD 명령어 확장입니다. SIMD는 Single Instruction, Multiple Data의 약자로, 하나의 명령으로 여러 숫자를 동시에 처리합니다. 이미지 필터, 행렬 연산, 벡터 내적처럼 같은 연산을 대량 반복하는 작업에서 유리합니다.

### 역사

SSE는 1999년 Pentium III와 함께 등장했고, 이후 SSE2, SSE3, SSSE3, SSE4로 확장되었습니다. 딥러닝 이전에도 영상 처리와 게임 물리 연산에서 널리 사용되었습니다.

### 사용하는 곳

OpenCV, BLAS, NumPy 내부 루틴, 비디오 코덱, 오디오 처리, 전통적 컴퓨터 비전 알고리즘에 사용됩니다.

### 최신 트렌드

최신 CPU에서는 SSE보다 AVX/AVX2/AVX-512가 더 큰 벡터 폭을 제공하지만, SSE는 호환성이 좋아 기본 경로로 남아 있습니다. 라이브러리들은 실행 시 CPU 기능을 감지해 SSE, AVX, AVX2 등을 자동 선택합니다.

### 참고 자료

- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
- [OpenCV CPU optimizations](https://docs.opencv.org/4.x/db/de0/group__core__utils.html)

## AVX(Intel Advanced Vector Extensions)

### 용어 및 기초 지식

AVX는 x86 CPU의 256-bit SIMD 확장입니다. SSE보다 한 번에 더 많은 float 데이터를 처리할 수 있어 행렬 곱, 컨볼루션, 이미지 전처리에서 성능을 높입니다.

### 역사

AVX는 2011년 Intel Sandy Bridge 세대부터 널리 쓰이기 시작했습니다. 이후 FMA, AVX2, AVX-512로 발전했습니다.

### 사용하는 곳

딥러닝 inference 엔진, OpenCV DNN, ONNX Runtime CPU Execution Provider, Intel oneDNN, 과학 계산 라이브러리에 사용됩니다.

### 최신 트렌드

CPU inference에서는 단순히 명령어를 쓰는 것보다 메모리 접근, 캐시 효율, int8/bfloat16 같은 저정밀 연산을 함께 최적화하는 방향이 중요합니다.

### 참고 자료

- [Intel oneDNN](https://github.com/oneapi-src/oneDNN)
- [ONNX Runtime CPU Execution Provider](https://onnxruntime.ai/docs/execution-providers/CPU-ExecutionProvider.html)

## AVX2

### 용어 및 기초 지식

AVX2는 AVX에 정수 벡터 연산과 gather 같은 기능을 확장한 명령어 집합입니다. 이미지 픽셀 처리와 양자화된 딥러닝 모델에서 특히 중요합니다.

### 역사

AVX2는 2013년 Intel Haswell 세대부터 본격적으로 보급되었습니다. 많은 데스크톱과 서버 CPU에서 현재도 널리 지원됩니다.

### 사용하는 곳

int8 inference, 이미지 resize/normalize, 행렬 곱, 특징 추출, 비디오 처리에 사용됩니다.

### 최신 트렌드

서버에서는 AVX-512나 AMX가 더 높은 성능을 제공할 수 있지만, 일반 PC와 엣지 장비에서는 AVX2가 여전히 중요한 기준선입니다.

### 참고 자료

- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
- [OpenVINO Documentation](https://docs.openvino.ai/)

## NEON

### 용어 및 기초 지식

NEON은 ARM CPU의 SIMD 확장입니다. Raspberry Pi, Jetson의 ARM 코어, 스마트폰, 임베디드 보드에서 벡터 연산을 빠르게 처리합니다.

### 역사

NEON은 ARMv7 시기부터 모바일 멀티미디어 가속을 위해 쓰였고, ARMv8/AArch64 환경에서 기본적인 성능 최적화 수단이 되었습니다.

### 사용하는 곳

모바일 비전 앱, 로봇 엣지 보드, 드론, 카메라 장치, TensorFlow Lite, OpenCV ARM 빌드에서 사용됩니다.

### 최신 트렌드

엣지 AI에서는 GPU/NPU가 주목받지만, 전처리와 작은 모델 inference는 여전히 CPU NEON 최적화의 영향을 많이 받습니다.

### 참고 자료

- [Arm Neon Programmer's Guide](https://developer.arm.com/documentation/102159/latest/)
- [TensorFlow Lite Performance](https://www.tensorflow.org/lite/performance)

## CUDA

### 용어 및 기초 지식

CUDA는 NVIDIA GPU에서 병렬 계산을 수행하기 위한 플랫폼과 프로그래밍 모델입니다. 딥러닝 학습과 대규모 inference에서 GPU의 수천 개 코어를 활용합니다.

### 역사

CUDA는 2006년 공개되었고, 2010년대 딥러닝 붐과 함께 cuDNN, TensorRT, PyTorch, TensorFlow의 핵심 가속 기반이 되었습니다.

### 사용하는 곳

모델 학습, 대량 이미지 inference, 자율주행 데이터 처리, 3D 비전, 로봇 시뮬레이션, SLAM 가속에 사용됩니다.

### 최신 트렌드

대형 모델에서는 FP16, BF16, INT8, FP8 등 저정밀 연산과 Tensor Core 활용이 중요합니다. 로봇/엣지에서는 Jetson 계열 장비와 TensorRT 배포가 자주 사용됩니다.

### 참고 자료

- [NVIDIA CUDA Toolkit Documentation](https://docs.nvidia.com/cuda/)
- [NVIDIA TensorRT Documentation](https://docs.nvidia.com/deeplearning/tensorrt/)

## forward(순전파)

### 용어 및 기초 지식

forward 또는 순전파는 입력 데이터가 신경망의 각 층을 지나 출력으로 변환되는 계산 과정입니다. 학습 중에는 forward 결과로 loss를 계산하고, inference 중에는 forward 결과 자체가 예측값이 됩니다.

### 역사

순전파는 다층 퍼셉트론부터 모든 신경망의 기본 계산 방식입니다. 역전파(backpropagation)가 학습을 담당한다면, 순전파는 예측 계산을 담당합니다.

### 사용하는 곳

이미지 분류 점수 계산, 객체 박스 예측, 세그멘테이션 마스크 생성, 로봇 행동 정책 출력에 사용됩니다.

### 최신 트렌드

추론 최적화에서는 forward 그래프를 컴파일하거나 fusion하여 연산 수와 메모리 이동을 줄입니다. ONNX Runtime, TensorRT, OpenVINO가 이런 최적화를 수행합니다.

### 참고 자료

- [Dive into Deep Learning: Multilayer Perceptrons](https://d2l.ai/chapter_multilayer-perceptrons/index.html)
- [ONNX Runtime Graph Optimizations](https://onnxruntime.ai/docs/performance/model-optimizations/graph-optimizations.html)

## inference(추론)

### 용어 및 기초 지식

inference는 학습된 모델을 사용해 새 입력에 대한 예측을 수행하는 단계입니다. 로봇 카메라 프레임에서 물체를 찾거나, 센서값으로 다음 행동을 판단하는 과정이 추론입니다.

### 역사

초기에는 학습과 추론이 같은 프레임워크에서 실행되는 경우가 많았지만, 서비스와 엣지 배포가 중요해지면서 전용 inference runtime이 발전했습니다.

### 사용하는 곳

실시간 객체 검출, 품질 검사, 로봇 팔 grasping, 자동 운전 보조, 모바일 AI 기능에 사용됩니다.

### 최신 트렌드

모델 경량화, quantization, pruning, distillation, hardware-aware compilation, edge/cloud 하이브리드 추론이 핵심입니다. 비전-language 모델도 작은 specialist model과 함께 배치되는 경우가 늘고 있습니다.

### 참고 자료

- [ONNX Runtime](https://onnxruntime.ai/docs/)
- [OpenVINO Performance Optimization](https://docs.openvino.ai/)
- [TensorRT](https://docs.nvidia.com/deeplearning/tensorrt/)

## Deeplearning framework

딥러닝 프레임워크는 모델을 만들고 학습하고 배포하는 도구입니다. 연구 단계에서는 PyTorch/TensorFlow가 많이 쓰이고, 배포 단계에서는 ONNX Runtime, TensorRT, OpenVINO, TensorFlow Lite 같은 runtime이 함께 쓰입니다.

## 모델/프레임워크 설정 파일

딥러닝 모델을 배포할 때는 보통 "네트워크 구조", "학습된 가중치", "입출력 전처리/후처리 정보"가 필요합니다. 프레임워크마다 이 세 가지를 한 파일에 넣기도 하고, 여러 파일로 나누기도 합니다.

### 전체 구조

| 프레임워크/포맷 | 구조 파일 | 가중치 파일 | 보조 설정 | 특징 |
| --- | --- | --- | --- | --- |
| Caffe | `.prototxt` | `.caffemodel` | `labels.txt`, mean 파일 | 구조와 가중치가 분리됨 |
| Darknet/YOLO | `.cfg` | `.weights` | `.names`, `.data` | YOLOv1/v2/v3 계열에서 자주 보임 |
| TensorFlow 1.x | `.pb`, `.pbtxt` | `.ckpt-*` 또는 frozen `.pb` 내부 | label map `.pbtxt` | 학습 checkpoint와 추론 graph가 분리될 수 있음 |
| TensorFlow 2.x | `saved_model.pb` | `variables/` 폴더 | `assets/`, signatures | SavedModel 디렉터리 단위 배포 |
| TensorFlow Lite | `.tflite` | `.tflite` 내부 | labels, metadata | 모바일/엣지용 단일 파일이 많음 |
| ONNX | `.onnx` | `.onnx` 내부 또는 external data | metadata, labels | 구조와 가중치를 보통 한 파일에 저장 |
| OpenVINO IR | `.xml` | `.bin` | `.mapping`, labels | `.xml`은 구조, `.bin`은 가중치 |
| PyTorch | Python 코드 또는 exported graph | `.pt`, `.pth`, `.safetensors` | `yaml`, class names | 학습 코드 의존성이 남는 경우가 많음 |
| Ultralytics YOLO | 모델 `.pt`, export `.onnx` 등 | 파일 내부 | dataset `.yaml` | 학습/검증 설정은 YAML을 많이 사용 |

### Caffe: `prototxt`와 `caffemodel`

Caffe 모델은 보통 두 파일로 나뉩니다.

- `deploy.prototxt`: 추론용 네트워크 구조입니다. layer 이름, layer 타입, 입력 크기, convolution kernel, stride, padding, 출력 blob 이름 등이 들어갑니다.
- `train_val.prototxt`: 학습용 구조입니다. data layer, loss layer, 학습/검증 입력 설정이 포함됩니다.
- `.caffemodel`: 학습된 weight와 bias가 들어 있는 바이너리 파일입니다.
- `solver.prototxt`: 학습률, momentum, weight decay, snapshot 주기 같은 학습 하이퍼파라미터가 들어갑니다.

OpenCV DNN에서 Caffe 모델을 읽을 때는 보통 아래처럼 구조 파일과 가중치 파일을 같이 넘깁니다.

```python
net = cv2.dnn.readNetFromCaffe("deploy.prototxt", "model.caffemodel")
```

간단한 `prototxt` layer 예시는 다음과 같습니다.

```protobuf
layer {
  name: "conv1"
  type: "Convolution"
  bottom: "data"
  top: "conv1"
  convolution_param {
    num_output: 96
    kernel_size: 11
    stride: 4
  }
}
```

여기서 `bottom`은 입력 blob, `top`은 출력 blob입니다. 여러 layer가 `top`/`bottom` 이름으로 연결되어 전체 계산 그래프를 만듭니다.

### Darknet/YOLO: `cfg`, `weights`, `names`, `data`

Darknet 기반 YOLO는 설정 파일이 비교적 사람이 읽기 쉬운 INI 형태입니다.

- `.cfg`: 네트워크 구조와 학습 설정입니다. `[net]`, `[convolutional]`, `[route]`, `[shortcut]`, `[yolo]` 같은 블록이 들어갑니다.
- `.weights`: 학습된 weight가 들어 있는 바이너리 파일입니다.
- `.names`: 클래스 이름 목록입니다. 예: `person`, `car`, `dog`.
- `.data`: 학습/검증 데이터 경로, class 수, names 파일 경로, backup 경로를 적습니다.

OpenCV DNN에서는 아래처럼 읽습니다.

```python
net = cv2.dnn.readNetFromDarknet("yolov3.cfg", "yolov3.weights")
```

YOLO `.cfg`의 핵심 블록 예시는 다음과 같습니다.

```ini
[net]
width=416
height=416
channels=3
batch=64
subdivisions=16

[convolutional]
filters=255
size=1
stride=1
pad=1
activation=linear

[yolo]
classes=80
num=3
anchors=10,13, 16,30, 33,23
```

주의할 점은 `[yolo]` 앞 convolution의 `filters` 값입니다. YOLOv3 기준으로 보통 `filters = (classes + 5) * anchors_per_scale`입니다. COCO 80 클래스, anchor 3개면 `(80 + 5) * 3 = 255`가 됩니다. custom dataset으로 class 수를 바꾸면 이 값도 같이 바꿔야 합니다.

### TensorFlow: GraphDef, Checkpoint, SavedModel, Lite

TensorFlow는 버전과 배포 방식에 따라 파일 형태가 달라집니다.

- `.pb`: GraphDef 또는 frozen graph입니다. TensorFlow 1.x 객체 검출 모델에서 자주 보입니다.
- `.pbtxt`: 사람이 읽을 수 있는 graph 또는 label map입니다. 객체 검출 API에서는 class id와 이름을 매핑하는 label map으로 자주 쓰입니다.
- `.ckpt-*`: 학습 중 저장되는 checkpoint입니다. `model.ckpt.index`, `model.ckpt.data-*`, `checkpoint` 파일이 함께 있습니다.
- `SavedModel/`: TensorFlow 2.x의 표준 배포 디렉터리입니다. `saved_model.pb`, `variables/`, `assets/`를 포함합니다.
- `.tflite`: 모바일/엣지 배포용 flatbuffer 파일입니다. 구조와 가중치를 한 파일에 담습니다.

OpenCV DNN에서 frozen graph를 읽을 때는 보통 아래처럼 사용합니다.

```python
net = cv2.dnn.readNetFromTensorflow("frozen_inference_graph.pb", "graph.pbtxt")
```

TensorFlow 객체 검출 label map 예시는 다음과 같습니다.

```protobuf
item {
  id: 1
  name: "person"
}
```

### ONNX: 단일 모델 파일과 external data

ONNX는 구조와 가중치를 보통 `.onnx` 한 파일에 저장합니다. 내부에는 graph, node, initializer, input/output tensor 정보가 들어갑니다.

- graph: 전체 계산 그래프입니다.
- node: `Conv`, `Relu`, `MatMul`, `Resize`, `NonMaxSuppression` 같은 연산입니다.
- initializer: 학습된 weight/bias입니다.
- opset: 어떤 버전의 연산 규격을 쓰는지 나타냅니다.
- metadata: producer, domain, custom metadata 등을 담을 수 있습니다.

모델이 매우 크면 weight를 외부 파일로 분리하는 external data 형식을 사용할 수 있습니다. 이 경우 `.onnx`만 옮기면 실행이 안 되고, 연결된 weight 파일도 같이 있어야 합니다.

ONNX Runtime 로딩은 다음과 같습니다.

```python
session = onnxruntime.InferenceSession("model.onnx")
outputs = session.run(None, {"input": input_tensor})
```

### OpenVINO: IR `xml`과 `bin`

OpenVINO IR(Intermediate Representation)은 구조와 가중치가 분리됩니다.

- `.xml`: layer, tensor shape, precision, graph 연결 정보가 들어 있습니다.
- `.bin`: 학습된 weight가 들어 있는 바이너리 파일입니다.
- `.mapping`: 원본 프레임워크 layer 이름과 IR layer 이름 매핑에 쓰일 수 있습니다.

OpenVINO Runtime에서는 다음처럼 읽습니다.

```python
core = openvino.Core()
model = core.read_model("model.xml", "model.bin")
compiled = core.compile_model(model, "CPU")
```

OpenCV DNN에서도 OpenVINO IR을 읽을 수 있습니다.

```python
net = cv2.dnn.readNet("model.xml", "model.bin")
```

### PyTorch와 Ultralytics YOLO 설정 파일

PyTorch는 프레임워크 자체가 Python 코드 중심이라 Caffe/Darknet처럼 항상 별도의 구조 설정 파일이 있는 것은 아닙니다.

- `.pth`, `.pt`: 모델 전체 또는 `state_dict`를 저장합니다. `state_dict`만 저장한 경우 모델 클래스 Python 코드가 따로 필요합니다.
- `.safetensors`: 안전하고 빠른 tensor 저장 포맷입니다. Hugging Face 생태계에서 자주 보입니다.
- `.yaml`: Ultralytics YOLO에서 dataset 경로, class 이름, 모델 구조, 학습 설정에 사용됩니다.

Ultralytics dataset YAML 예시는 다음과 같습니다.

```yaml
path: ./datasets/parts
train: images/train
val: images/val
names:
  0: bolt
  1: nut
  2: washer
```

YOLO 모델 구조 YAML은 backbone/head를 정의하며, 학습된 `.pt` 파일은 모델 구조와 weight를 함께 들고 있는 경우가 많습니다. 배포 시에는 `.onnx`, TensorRT engine, OpenVINO IR 등으로 export하는 경우가 많습니다.

### 모델별로 자주 만나는 설정 파일

| 모델 | 자주 만나는 파일 형태 | 설명 |
| --- | --- | --- |
| AlexNet | Caffe `deploy.prototxt` + `.caffemodel`, PyTorch `.pth` | 초기 공개 모델은 Caffe 형식이 많고, 학습 실습은 PyTorch 구현이 많습니다. |
| GoogLeNet | Caffe `deploy.prototxt` + `.caffemodel`, TensorFlow/PyTorch checkpoint | Inception 계열 변형이 많아 정확한 버전 이름을 확인해야 합니다. |
| ResNet | PyTorch `.pth`, ONNX `.onnx`, TensorFlow SavedModel | backbone으로 많이 쓰이므로 detection/segmentation 설정 파일 안에 ResNet 이름이 들어가는 경우도 많습니다. |
| SSD | Caffe `deploy.prototxt` + `.caffemodel`, TensorFlow `.pb`/`.pbtxt`, ONNX `.onnx` | OpenCV DNN 예제에서 Caffe SSD MobileNet 조합을 자주 볼 수 있습니다. |
| YOLO | Darknet `.cfg` + `.weights` + `.names`, Ultralytics `.pt` + `.yaml`, ONNX `.onnx` | YOLOv3 이전 자료는 Darknet 형식, 최신 실무 자료는 PyTorch/Ultralytics/export 포맷이 많습니다. |
| Faster R-CNN | TensorFlow `.pb` + label map `.pbtxt`, PyTorch `.pth`, Detectron2 config `.yaml` | two-stage detector라 backbone, RPN, ROI head 설정이 분리되어 표현되는 경우가 많습니다. |
| OpenVINO | `.xml` + `.bin`, 또는 원본 ONNX/TensorFlow에서 변환 | OpenVINO는 모델 이름이라기보다 배포 toolkit이므로 IR 파일 쌍을 확인하는 것이 중요합니다. |

### 설정 파일을 읽을 때 체크할 것

- 입력 크기: `224x224`, `300x300`, `416x416`, dynamic shape 등 모델이 기대하는 입력 크기를 확인합니다.
- 색상 순서: OpenCV는 기본 BGR, 대부분 딥러닝 학습 코드는 RGB를 사용합니다.
- 정규화: `0~1` 스케일, mean/std, mean subtraction 여부를 확인합니다.
- 클래스 수: YOLO/SSD/Faster R-CNN의 output channel, label map, names 파일의 class 수가 일치해야 합니다.
- 후처리: NMS가 모델 내부에 있는지, runtime 밖에서 해야 하는지 확인합니다.
- opset/버전: ONNX opset, TensorFlow 버전, OpenVINO IR 버전이 runtime과 호환되는지 확인합니다.
- 동적 입력: batch, width, height가 dynamic이면 runtime에서 shape 설정이 필요한 경우가 있습니다.

## ONNX

### 용어 및 기초 지식

ONNX(Open Neural Network Exchange)는 서로 다른 딥러닝 프레임워크 사이에서 모델을 교환하기 위한 공개 포맷입니다. PyTorch로 학습한 모델을 ONNX로 내보낸 뒤 ONNX Runtime, OpenVINO, TensorRT 등에서 실행할 수 있습니다.

### 역사

ONNX는 2017년 Microsoft와 Facebook이 중심이 되어 공개했습니다. 이후 다양한 프레임워크와 runtime이 지원하면서 모델 배포의 중간 표현으로 자리 잡았습니다.

### 사용하는 곳

모델 배포, inference 최적화, 프레임워크 간 변환, C++/C#/Python 서비스 통합에 사용됩니다.

### 최신 트렌드

ONNX Runtime은 분기별 릴리스 체계를 유지하며 CPU, CUDA, TensorRT, DirectML 등 다양한 Execution Provider를 지원합니다. 2026년에는 전통적인 비전 모델뿐 아니라 생성형 AI와 엣지 가속 배포에도 활용 범위가 넓어지고 있습니다.

### 참고 자료

- [ONNX](https://onnx.ai/)
- [ONNX Runtime Documentation](https://onnxruntime.ai/docs/)
- [ONNX Runtime Releases](https://onnxruntime.ai/docs/reference/releases-servicing.html)

## Caffe

### 용어 및 기초 지식

Caffe는 Berkeley Vision and Learning Center에서 만든 딥러닝 프레임워크입니다. `prototxt`로 네트워크 구조를 정의하고 `.caffemodel`에 학습된 가중치를 저장합니다.

### 역사

2014년 전후 컴퓨터 비전 연구와 산업 프로젝트에서 널리 쓰였습니다. AlexNet, SSD 같은 초기 CNN 모델 배포 자료가 Caffe 형식으로 많이 공개되었습니다.

### 사용하는 곳

레거시 비전 모델 실행, OpenCV DNN 샘플, 오래된 연구 모델 재현에 사용됩니다.

### 최신 트렌드

새 프로젝트에서는 PyTorch, TensorFlow, ONNX가 더 일반적입니다. Caffe는 신규 학습보다 기존 모델을 변환하거나 유지보수할 때 주로 만납니다.

### 참고 자료

- [Caffe](https://caffe.berkeleyvision.org/)
- [Caffe GitHub](https://github.com/BVLC/caffe)

## Darknet

### 용어 및 기초 지식

Darknet은 Joseph Redmon이 만든 C 기반 딥러닝 프레임워크입니다. YOLO 초기 버전이 Darknet 기반으로 공개되면서 객체 검출 분야에서 유명해졌습니다.

### 역사

YOLOv1, YOLOv2, YOLOv3가 Darknet과 함께 널리 사용되었습니다. 간단한 설정 파일과 빠른 실행 속도 덕분에 연구자와 개발자가 쉽게 실험할 수 있었습니다.

### 사용하는 곳

YOLO 레거시 모델 학습/추론, 임베디드 객체 검출 실험, OpenCV DNN에서 Darknet `.cfg`/`.weights` 모델 로딩에 사용됩니다.

### 최신 트렌드

최신 YOLO 계열은 PyTorch 기반 구현과 Ultralytics 생태계가 주류입니다. Darknet은 레거시 YOLO 모델을 이해하거나 변환할 때 유용합니다.

### 참고 자료

- [Darknet](https://pjreddie.com/darknet/)
- [YOLOv3 Paper](https://arxiv.org/abs/1804.02767)

## TensorFlow

### 용어 및 기초 지식

TensorFlow는 Google이 공개한 딥러닝 프레임워크입니다. 학습, 배포, 모바일/브라우저 실행까지 포함하는 생태계를 제공합니다.

### 역사

TensorFlow는 2015년 공개되었고, 2019년 TensorFlow 2.x에서 eager execution과 Keras 중심 API로 사용성이 개선되었습니다.

### 사용하는 곳

대규모 학습, TensorFlow Lite 모바일/엣지 배포, TensorFlow Serving, 웹 기반 TensorFlow.js에 사용됩니다.

### 최신 트렌드

엣지 배포에서는 TensorFlow Lite가 여전히 중요하며, 연구에서는 PyTorch 비중이 커졌습니다. 실제 서비스에서는 학습 프레임워크와 배포 runtime을 분리해 ONNX, TensorRT, OpenVINO로 변환하는 경우도 많습니다.

### 참고 자료

- [TensorFlow](https://www.tensorflow.org/)
- [TensorFlow Lite](https://www.tensorflow.org/lite)

## Deeplearning model

딥러닝 모델은 특정 문제를 풀기 위해 설계된 네트워크 구조입니다. 아래 모델들은 컴퓨터 비전과 로봇 비전의 발전 흐름을 이해하는 데 중요한 기준점입니다.

## AlexNet

### 용어 및 기초 지식

AlexNet은 깊은 CNN을 GPU로 학습해 ImageNet 분류 성능을 크게 끌어올린 모델입니다. ReLU, dropout, data augmentation, GPU 병렬 학습을 효과적으로 사용했습니다.

### 역사

2012년 ImageNet Large Scale Visual Recognition Challenge에서 압도적인 성능을 보이며 딥러닝 기반 컴퓨터 비전의 전환점이 되었습니다.

### 사용하는 곳

현재 실무 모델로는 거의 쓰이지 않지만 CNN 역사, feature map, convolution/pooling 구조를 배우는 데 좋습니다.

### 최신 트렌드

현대 모델은 AlexNet보다 훨씬 깊고 효율적이며, CNN과 Transformer를 혼합하거나 foundation model을 활용합니다. AlexNet은 기준 역사 모델로 남아 있습니다.

### 참고 자료

- [AlexNet Paper](https://papers.nips.cc/paper/4824-imagenet-classification-with-deep-convolutional-neural-networks)

## GoogLeNet

### 용어 및 기초 지식

GoogLeNet은 Inception 구조를 사용해 여러 크기의 convolution을 병렬로 적용하고 결과를 합치는 CNN입니다. 파라미터 수를 줄이면서 깊이를 늘린 것이 특징입니다.

### 역사

2014년 ImageNet 대회에서 좋은 성능을 보였고, Inception 계열 모델의 시작점이 되었습니다.

### 사용하는 곳

이미지 분류, feature extractor, 경량 CNN 구조 학습에 사용됩니다.

### 최신 트렌드

Inception 아이디어는 효율적인 multi-scale feature 처리로 이어졌고, 현대 객체 검출/세그멘테이션 모델에서도 multi-scale feature pyramid 형태로 계승되었습니다.

### 참고 자료

- [Going Deeper with Convolutions](https://arxiv.org/abs/1409.4842)

## ResNet

### 용어 및 기초 지식

ResNet은 residual connection을 사용해 매우 깊은 신경망을 안정적으로 학습하게 만든 CNN입니다. 입력을 몇 개의 층 뒤 출력에 더하는 skip connection이 핵심입니다.

### 역사

2015년 공개되어 ImageNet과 COCO 등에서 강력한 성능을 보였고, 이후 거의 모든 비전 모델 설계에 영향을 주었습니다.

### 사용하는 곳

분류, 검출 backbone, 세그멘테이션, 로봇 비전 feature extractor에 사용됩니다.

### 최신 트렌드

Transformer 계열이 커졌지만 ResNet은 여전히 강력한 baseline입니다. 경량화된 ResNet, ConvNeXt, hybrid backbone 등으로 아이디어가 이어지고 있습니다.

### 참고 자료

- [Deep Residual Learning for Image Recognition](https://arxiv.org/abs/1512.03385)

## SSD

### 용어 및 기초 지식

SSD(Single Shot MultiBox Detector)는 이미지를 한 번 forward하여 여러 scale feature map에서 객체 박스와 클래스를 동시에 예측하는 one-stage detector입니다.

### 역사

2016년 공개되었고, Faster R-CNN보다 빠른 실시간 검출을 목표로 했습니다. 모바일/임베디드 검출 모델의 중요한 기반이 되었습니다.

### 사용하는 곳

실시간 객체 검출, 모바일 비전, OpenCV DNN 샘플, 로봇 카메라 인식에 사용됩니다.

### 최신 트렌드

SSD 자체보다 YOLO, EfficientDet, RT-DETR 계열이 더 많이 쓰이지만, anchor box와 multi-scale detection 개념을 이해하기 좋은 모델입니다.

### 참고 자료

- [SSD Paper](https://arxiv.org/abs/1512.02325)

## YOLO

### 용어 및 기초 지식

YOLO(You Only Look Once)는 이미지를 격자로 보고 한 번의 forward로 객체 위치와 클래스를 예측하는 실시간 객체 검출 모델 계열입니다.

### 역사

YOLOv1은 2015년 공개되었고, YOLOv2/v3는 Darknet 기반으로 널리 쓰였습니다. 이후 다양한 연구와 구현체가 등장했고, Ultralytics YOLO는 실무에서 많이 사용되는 생태계가 되었습니다.

### 사용하는 곳

로봇 객체 인식, 안전 감시, 제조 검사, 교통 분석, 드론 영상 분석, 실시간 카메라 앱에 사용됩니다.

### 최신 트렌드

2026년 기준 Ultralytics 문서는 YOLO26을 최신 모델로 소개하며 edge-first 배포, NMS-free inference, 검출/분할/포즈/OBB 등 다중 태스크 지원을 강조합니다. 실무에서는 정확도, 지연시간, 배포 대상 하드웨어를 함께 보고 YOLO11/YOLO26, RT-DETR, custom lightweight model을 비교합니다.

### 참고 자료

- [YOLOv1 Paper](https://arxiv.org/abs/1506.02640)
- [Ultralytics YOLO Documentation](https://docs.ultralytics.com/)
- [Ultralytics Models](https://docs.ultralytics.com/models/)

## Faster R-CNN

### 용어 및 기초 지식

Faster R-CNN은 region proposal network(RPN)로 후보 영역을 만들고, 각 영역을 분류/보정하는 two-stage detector입니다. 속도보다 정확도와 안정성이 중요한 검출에서 강점이 있습니다.

### 역사

R-CNN, Fast R-CNN의 병목이던 후보 영역 생성 과정을 신경망 내부로 통합해 2015년에 공개되었습니다.

### 사용하는 곳

정확도 중심 객체 검출, 데이터셋 기준 성능 비교, 의료/산업 영상 분석, 작은 객체 검출 연구에 사용됩니다.

### 최신 트렌드

실시간 시스템에서는 one-stage detector나 transformer detector가 선호되지만, two-stage 구조는 여전히 높은 정확도 baseline과 연구 기준으로 의미가 있습니다.

### 참고 자료

- [Faster R-CNN Paper](https://arxiv.org/abs/1506.01497)

## OpenVINO

### 용어 및 기초 지식

OpenVINO는 Intel 하드웨어에서 AI 모델을 최적화하고 배포하기 위한 오픈소스 toolkit입니다. CPU, GPU, NPU 등 Intel 장치에서 모델을 효율적으로 실행할 수 있게 돕습니다.

### 역사

OpenVINO는 Intel Computer Vision SDK에서 발전했고, 이후 다양한 프레임워크 모델을 변환/최적화하는 배포 도구로 확장되었습니다.

### 사용하는 곳

산업용 PC, 엣지 AI 카메라, 로봇 비전, 제조 검사, CPU 기반 inference 서버, Intel NPU가 있는 PC에서 사용됩니다.

### 최신 트렌드

공식 문서 기준 OpenVINO 2026.2가 제공되며, Intel 다운로드 페이지는 2026.2를 최신 버전으로 안내합니다. 최근 방향은 ONNX/TensorFlow/Paddle 모델 호환, GenAI 통합, 모델 압축, 엣지 배포 성능 개선입니다.

### 참고 자료

- [OpenVINO Documentation](https://docs.openvino.ai/)
- [Install OpenVINO 2026.2](https://docs.openvino.ai/install)
- [Intel OpenVINO Toolkit Download](https://www.intel.com/content/www/us/en/download/753640/intel-distribution-of-openvino-toolkit.html)
