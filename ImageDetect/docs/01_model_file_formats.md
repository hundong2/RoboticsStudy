# 01. 모델 파일 형식

객체 탐지 모델은 학습 프레임워크와 배포 환경에 따라 파일 확장자가 달라집니다. 실무에서는 원본 학습 포맷을 그대로 쓰기보다, C#/C++/Java에서 다루기 쉬운 ONNX 또는 TFLite로 변환하는 일이 많습니다.

| 확장자 | 주 사용처 | 의미 | 이 커리큘럼에서의 위치 |
| --- | --- | --- | --- |
| `.cfg` | Darknet YOLO | 네트워크 구조 설정 파일 | 참고 개념 |
| `.weights` | Darknet YOLO | 학습된 가중치 | 참고 개념 |
| `.names`, `.txt` | 공통 | 클래스 이름 목록 | `assets/classes/coco.names` |
| `.pt`, `.pth` | PyTorch | 모델 가중치 또는 체크포인트 | YOLOX, RT-DETR 원본 |
| `.pdparams` | PaddlePaddle | PaddleDetection 가중치 | RT-DETR Paddle 경로 |
| `.h5` | TensorFlow/Keras | Keras 모델/가중치 | EfficientDet 원본 |
| `.ckpt`, `.tar.gz`, `.tgz` | TensorFlow | 체크포인트 묶음 | EfficientDet 원본 |
| `.onnx` | ONNX Runtime, OpenCV DNN | 범용 추론 그래프 | C# OpenCVSharp 실습 |
| `.tflite` | TensorFlow Lite, MediaPipe | 모바일/엣지 추론 모델 | MediaPipe 실습 |

## C#에서 가장 편한 포맷

C#에서 빠르게 실습하려면 `.onnx`를 추천합니다.

- OpenCVSharp `CvDnn.ReadNetFromOnnx()`로 바로 로드할 수 있습니다.
- ONNX Runtime C# 패키지로도 실행할 수 있습니다.
- CPU 환경에서도 배포가 쉽고, GPU/TensorRT/OpenVINO 경로로 확장하기 좋습니다.

단, 같은 ONNX라도 모델마다 출력 텐서 구조가 다릅니다. YOLOX는 후보 박스와 클래스 점수 행렬을 직접 파싱해야 하고, RT-DETR은 export 방식에 따라 logits/boxes 또는 후처리된 결과가 나올 수 있습니다.

## TFLite가 좋은 경우

MediaPipe나 모바일/라즈베리파이 쪽은 `.tflite`가 편합니다. MediaPipe Tasks 모델은 메타데이터가 포함되어 있어 클래스 이름과 전처리/후처리 조건을 API가 일부 대신 처리합니다.
