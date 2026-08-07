# 02. C++ ML 로봇 파이프라인

목표는 [PyTorch](../glossary/README.md#pytorch)에서 학습한 모델을 [ONNX](../glossary/README.md#onnx)로 내보내고, [C++](../glossary/README.md#cpp) [ROS 2](../glossary/README.md#ros2) 노드에서 [추론](../glossary/README.md#inference)한 뒤 로봇 명령으로 연결하는 흐름을 이해하는 것입니다.

## 왜 C++ 추론 노드가 필요한가?

Python은 학습과 실험에 좋습니다. 그러나 실제 로봇 제어 루프에서는 지연 시간, 메모리, dependency, 배포 안정성이 중요합니다. 그래서 실무에서는 다음처럼 역할을 나눕니다.

| 역할 | 권장 언어 | 이유 |
|---|---|---|
| 데이터 수집/학습 | Python | PyTorch, 실험 도구, 빠른 반복 |
| 모델 export | Python | `torch.onnx.export`, 검증 스크립트 |
| 추론 노드 | C++ | 낮은 latency, 안정적인 배포 |
| LLM Agent | Python | LangChain/LlamaIndex 생태계 |
| safety/controller | C++ | 실시간성, 하드웨어 근접 |

## 표준 흐름

```text
train_policy.py
  -> policy.pt
  -> export_policy_onnx.py
  -> policy.onnx
  -> ml_policy_node.cpp
  -> /policy/cmd_vel_raw
  -> safety_filter.cpp
  -> /cmd_vel
```

## ONNX export 최소 예시

```python
# torch는 PyTorch의 최상위 패키지입니다.
import torch

# PolicyModel은 학습 때 사용한 모델 클래스라고 가정합니다.
from my_policy import PolicyModel

# 모델 객체를 생성합니다.
model = PolicyModel()

# 학습된 weight를 파일에서 읽습니다.
model.load_state_dict(torch.load("policy.pt", map_location="cpu"))

# eval은 dropout/batchnorm을 추론 모드로 바꿉니다.
model.eval()

# dummy_input은 ONNX graph를 tracing할 때 필요한 예시 입력입니다.
dummy_input = torch.randn(1, 360)

# torch.onnx.export는 PyTorch 모델을 ONNX 파일로 저장합니다.
torch.onnx.export(
    model,
    dummy_input,
    "policy.onnx",
    input_names=["scan"],
    output_names=["cmd_vel"],
    dynamic_axes={"scan": {0: "batch"}, "cmd_vel": {0: "batch"}},
    opset_version=18,
)
```

## C++ 예제 위치

- [ml_policy_node.cpp](../examples/cpp_ml_policy_node/src/ml_policy_node.cpp)
- [safety_filter.cpp](../examples/cpp_ml_policy_node/src/safety_filter.cpp)

예제는 초보자가 한 줄씩 읽을 수 있도록 주석을 과하게 넣었습니다. 실제 제품 코드에서는 주석을 줄이고 테스트와 타입 설계를 강화합니다.

## 실무 포인트

- 모델 입력 shape, normalization, frame, topic 이름을 문서화합니다.
- 모델 출력은 바로 actuator로 보내지 말고 [safety filter](../glossary/README.md#safety-filter)를 통과시킵니다.
- 추론 노드는 [parameter](../glossary/README.md#parameter)로 모델 경로, 속도 제한, 안전 거리, dry-run 여부를 받습니다.
- [rosbag2](../glossary/README.md#rosbag2)로 같은 센서 입력을 재생해 모델 업데이트 전후 출력을 비교합니다.
- [ONNX Runtime](../glossary/README.md#onnx-runtime)과 [TensorRT](../glossary/README.md#tensorrt)는 성능 최적화 단계에서 도입합니다.

## 통과 기준

- PyTorch checkpoint와 ONNX artifact의 차이를 설명한다.
- C++ ROS 2 노드에서 subscriber, publisher, timer, parameter가 어떤 역할을 하는지 설명한다.
- 모델 출력과 안전한 로봇 명령 사이에 검증 계층이 필요하다는 점을 설명한다.
