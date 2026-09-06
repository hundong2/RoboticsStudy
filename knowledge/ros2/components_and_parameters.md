# ROS 2 Components와 Parameters

## Component를 쓰는 이유

ROS 2 Component는 `rclcpp::Node` 구현을 공유 라이브러리 plugin으로 등록해 container 프로세스에 로드하는 방식이다. 프로세스 수와 serialization 비용을 줄이고 배포 시 노드 조합을 바꿀 수 있다.

필수 패턴:

```cpp
class CameraNode : public rclcpp::Node
{
public:
  explicit CameraNode(const rclcpp::NodeOptions & options)
  : Node("camera", options) {}
};

RCLCPP_COMPONENTS_REGISTER_NODE(CameraNode)
```

CMake에는 `rclcpp_components_register_nodes(target "CameraNode")`가 필요하다. 이름이 C++ 완전 수식 이름, launch의 `plugin`, component index에서 모두 일치해야 한다.

## 조합의 trade-off

- 장점: process/context 감소, intra-process 전달, 재사용 가능한 배포 조합
- 단점: 한 component crash가 container 전체에 영향, CPU/메모리 장애 격리 약화
- `component_container_mt`는 병렬 실행 가능성을 주지만 callback group 설계 없이는 우선순위와 동시성이 의도대로 되지 않는다.

## Parameter callback의 안전한 순서

1. `declare_parameter`: 타입, 기본값, override를 노드에 등록
2. `add_on_set_parameters_callback`: 저장 전 validation만 수행
3. Node parameter store commit
4. `add_post_set_parameters_callback`: 검증된 값을 runtime config에 반영

`on-set`에서 장치 I/O나 내부 상태 변경을 먼저 하면 뒤 callback의 거부와 불일치가 생길 수 있다. 여러 설정값이 한 덩어리로 일관되어야 하면 immutable config 두 벌 중 하나를 준비하고 atomic index/pointer 하나로 교체한다.

## 점검 질문

- 이 기능들을 같은 장애 도메인에 놓아도 되는가?
- intra-process가 실제로 켜졌는지 주소/trace/benchmark로 확인했는가?
- default MutuallyExclusive group이 의도한 직렬화를 만드는가?
- parameter 검증이 단위, 범위, 필드 간 invariant를 모두 검사하는가?

## 참고

- [ROS 2 Composition](https://docs.ros.org/en/rolling/Tutorials/Intermediate/Composition.html)
- [rclcpp Jazzy parameter callback API](https://docs.ros.org/en/jazzy/p/rclcpp/genindex.html)
