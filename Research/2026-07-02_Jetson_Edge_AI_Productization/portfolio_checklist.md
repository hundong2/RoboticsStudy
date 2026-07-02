# 포트폴리오 체크리스트

## 필수 증거

- [ ] Jetson Orin Nano에서 실행한 사진 또는 영상
- [ ] 카메라 입력 성공 화면
- [ ] pretrained model baseline 결과
- [ ] custom dataset 수집 과정
- [ ] fine-tuning 결과
- [ ] ONNX export 결과
- [ ] TensorRT 변환 결과
- [ ] FPS/latency/memory 리포트
- [ ] 오탐/미탐 분석
- [ ] 개선 방향

## README에 꼭 들어갈 내용

```text
Hardware
- Jetson Orin Nano
- Camera module
- Power mode

Software
- JetPack version
- Python version
- PyTorch version
- OpenCV version
- TensorRT version

Model
- Baseline model
- Dataset
- Classes
- Training config
- Evaluation metric

Deployment
- PyTorch latency
- ONNX Runtime latency
- TensorRT FP16 latency
- Camera FPS
- End-to-end latency

Analysis
- False positive examples
- False negative examples
- Bottleneck
- Next improvement
```

## 면접용 설명 포인트

좋은 답변:

```text
단순히 모델을 학습하는 데서 멈추지 않고, Jetson Orin Nano에서 카메라 입력부터 추론, 후처리, 성능 측정까지 연결했습니다. PyTorch 모델을 ONNX로 변환하고 TensorRT FP16 engine으로 최적화하면서 FPS와 latency 변화를 비교했습니다. 또한 오탐/미탐 이미지를 따로 저장해 threshold와 데이터셋 개선 방향을 분석했습니다.
```

피해야 할 답변:

```text
YOLO를 실행해봤습니다.
정확도가 잘 나왔습니다.
인터넷 예제를 따라했습니다.
```

차이는 "실행했다"가 아니라 "측정했고, 분석했고, 제품화 관점으로 개선했다"입니다.

