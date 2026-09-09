# ROS 2 Multi-Robot Namespace와 Remapping

## 목적

동일한 node executable을 여러 로봇에 재사용할 때 topic/service/action 이름 충돌을 막고, 소스 코드와 배포 graph 계약을 분리한다.

## 이름 세 종류

- **상대 이름** `scan`: node namespace가 `/robot_1`이면 `/robot_1/scan`으로 확장된다.
- **절대 이름** `/map`: namespace와 무관하게 항상 `/map`이다.
- **private 이름** `~/status`: node의 FQN이 `/robot_1/lidar`이면 `/robot_1/lidar/status`가 된다.

라이브러리/재사용 node는 상대 이름을 기본으로 쓰고, launch에서 namespace와 remapping을 정하는 편이 안전하다. fleet coordinator처럼 특정 global 계약을 가져야 할 때만 절대 이름을 제한적으로 사용한다.

## 확장 순서의 직관

ROS 2 static remapping에서는 node 이름/namespace 규칙이 먼저 적용되고 topic remapping이 이어진다. 실무에서는 최종 FQN을 launch test나 다음 명령으로 반드시 확인한다.

```bash
ros2 node list
ros2 node info /robot_1/frontier_explorer
ros2 topic list -t
```

## 다중 로봇 설계 체크리스트

1. Topic만이 아니라 TF frame(`robot_1/base_link`)과 parameter 파일도 로봇별로 격리한다.
2. `/tf`, `/tf_static`, `/clock`, fleet map처럼 의도적으로 공유하는 global interface를 문서화한다.
3. QoS는 이름이 연결된 뒤에도 양쪽 호환성이 맞아야 한다.
4. namespace는 네트워크 보안 경계가 아니다. SROS 2/DDS permission으로 publish/subscribe 권한을 별도 제한한다.
5. 같은 물리 좌표를 비교하려면 모든 goal이 공통 frame에 있어야 한다. 이름 분리만으로 좌표 정합은 해결되지 않는다.

## 참고

- [ROS 2 node arguments and remapping](https://docs.ros.org/en/jazzy/How-To-Guides/Node-arguments.html)
- [ROS 2 static remapping design](https://design.ros2.org/articles/static_remapping.html)
