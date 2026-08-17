# DeepStream에 custom ONNX 모델 연결

DeepStream은 GStreamer 위에서 카메라 입력, batch, TensorRT 추론, tracker, 화면 표시, 하드웨어 인코딩을 연결합니다. 카메라가 여러 대이거나 tracker와 video encode가 중요하면 직접 모든 단계를 구현하는 것보다 유리합니다.

## 필요한 파일

- `model.onnx`: ONNX 모델
- `labels.txt`: 클래스 번호 순서와 같은 label 목록
- `config_infer_primary.txt`: `nvinfer` 설정
- detector output이 표준 형식이 아니면 bounding-box parser 공유 라이브러리

예제는 `device/config/deepstream/config_infer_primary.example.txt`에 있습니다.

```ini
onnx-file=/opt/robot-vision/models/person-detector/0.1.0/model.onnx
model-engine-file=/opt/robot-vision/models/person-detector/0.1.0/model.fp16.engine
labelfile-path=/opt/robot-vision/models/person-detector/0.1.0/labels.txt
network-mode=2
```

`network-mode=2`는 FP16입니다. detector는 output tensor를 box/class/confidence로 바꾸는 parser가 필요할 수 있습니다. 모델별 parser 함수명과 `custom-lib-path`를 설정하지 않으면 engine 생성에는 성공해도 탐지 결과가 나오지 않을 수 있습니다.

## 적용 순서

1. `trtexec`로 ONNX parsing과 단독 추론이 되는지 확인합니다.
2. DeepStream `nvinfer`만 연결해 tensor/output을 확인합니다.
3. model-specific parser와 NMS 결과를 기준 구현과 비교합니다.
4. tracker를 추가합니다.
5. OSD와 NVENC/WebRTC 출력은 마지막에 추가합니다.

여러 문제를 한 번에 연결하면 모델 문제와 영상 pipeline 문제를 구분하기 어렵습니다.

공식 자료: [DeepStream custom model guide](https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_using_custom_model.html)

