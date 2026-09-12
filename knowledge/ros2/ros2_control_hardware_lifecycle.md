# ros2_control Hardware Lifecycle

## 책임 분리

`controller_manager`는 control loop와 controller lifecycle을 관리하고, `ResourceManager`는 pluginlib로 hardware component를 로드해 state/command interface와 hardware lifecycle을 관리한다. 기본 loop는 다음 순서다.

```text
hardware.read() -> active controllers.update() -> limit enforcement -> hardware.write()
```

System은 여러 관절과 복합 transmission을 한 통신 채널로 다룰 때, Actuator는 단일 구동기, Sensor는 읽기 전용 장치에 적합하다.

## 상태와 실제 장비 의미

| 상태 | software 의미 | 실제 장비에서 흔한 작업 |
|---|---|---|
| `unconfigured` | plugin은 로드됐지만 통신 준비 전 | parameter 검증, 자원 미점유 |
| `inactive` | state read 가능, motion command는 아직 허용하지 않음 | bus 연결, encoder 확인, brake 유지 |
| `active` | controller가 movement interface를 claim·command 가능 | servo on, brake release |
| `finalized` | shutdown 완료 | 전원/통신 안전 종료 |

`on_activate()`에서는 measured state를 command에 복사해 bumpless transfer를 만들고, `on_deactivate()`는 torque off/brake/hold 정책을 명시해야 한다. 서비스 성공만 믿지 말고 `ros2 control list_hardware_components`의 최종 state와 claimed interface를 확인한다.

## 구현 체크리스트

- `on_init()`에서 URDF joint와 interface 수·이름·type을 검증한다.
- Jazzy의 framework-managed interface handle을 사용하고, hot path용 handle은 export 단계에서 캐시한다.
- `read()`/`write()`에서는 allocation, blocking I/O, logger, exception을 피한다.
- 통신 timeout/CRC/drive fault 때 `ERROR`를 반환할 조건과 controller fallback을 시험한다.
- controller 활성화 전에 hardware가 `active`인지 확인한다.
- 종료·재활성화·통신 재접속을 정상 시작만큼 자주 시험한다.

## 공식 자료

- [ros2_control Getting Started — Architecture](https://control.ros.org/jazzy/doc/getting_started/getting_started.html)
- [Writing a Hardware Component](https://control.ros.org/jazzy/doc/ros2_control/hardware_interface/doc/writing_new_hardware_component.html)
- [Controller Manager](https://control.ros.org/jazzy/doc/ros2_control/controller_manager/doc/userdoc.html)
