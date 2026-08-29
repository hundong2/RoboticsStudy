# 8. 임베디드 Vision VLA 목표와 로드맵

## 8.1 목표 정의

최종 목표는 카메라 영상과 자연어 지시, 로봇 상태를 입력으로 받아 검증된 행동을 출력하는 임베디드 시스템이다. Orin Nano 8GB에서는 단일 거대 end-to-end VLA보다 계층형 구조가 안전하고 구현 가능성이 높다.

```text
카메라/상태
  -> 실시간 Vision 인식
  -> 저주기 VLM 의미 추론·계획
  -> 제한된 action schema
  -> 정책/궤적 생성
  -> 독립 safety supervisor
  -> MCU/actuator
```

## 8.2 성공 기준

- 기능: 정해진 5~10개 작업을 언어 지시로 수행한다.
- 지연: 제어 루프는 고정 주기, VLM은 비동기 저주기로 분리한다.
- 안전: 사람/충돌/통신 손실/모델 timeout에서 정지한다.
- 재현성: 동일 software manifest와 calibration으로 다시 설치할 수 있다.
- 관측성: episode마다 영상, 상태, 지시, 후보 행동, 거부 이유를 기록한다.
- 배포성: 부팅 후 자동 시작, watchdog, OTA rollback이 가능하다.

## 8.3 단계별 마일스톤

### M0. 보드 기준선

- JetPack 7.2.1 설치와 NVMe 부팅
- MAXN SUPER/25W 모드 비교
- `tegrastats` 기록
- 안전 종료 버튼과 원격 SSH
- 결과: 2시간 연속 부하에서 오류·열 스로틀링 상태를 설명할 수 있음

### M1. 카메라 기준선

- CSI 또는 USB 카메라 1대
- 고정 해상도/FPS, timestamp, calibration
- GStreamer/NVMM 파이프라인
- 결과: 30분 캡처에서 frame drop과 latency 측정

### M2. 실시간 perception

- detector/segmenter/depth 중 작업에 필요한 1~2개만 TensorRT로 배포
- ROS 2 topic과 TF 연결
- 결과: sensor-to-result p95 latency와 작업별 정확도 확보

### M3. 언어 기반 목표 해석

- 소형 VLM/LLM이 자연어를 자유 텍스트 대신 제한된 JSON action schema로 변환
- 허용 동작, 좌표, 속도, confidence, expiry 포함
- 결과: 잘못된 JSON, 미지원 지시, 불확실한 지시를 거부

### M4. 행동 정책

- 먼저 규칙/상태기계 또는 작은 policy network로 action primitive 실행
- pick, place, move, stop 같은 primitive를 ROS action으로 구현
- 결과: VLM이 직접 PWM/속도 명령을 만들지 않음

### M5. imitation/VLA

- teleoperation episode 수집
- x86 GPU에서 behavior cloning 또는 경량 VLA fine-tuning
- Jetson용 양자화·TensorRT/지원 런타임 변환
- 결과: 고정 평가 세트에서 성공률, 개입률, 안전 거부율 측정

### M6. 제품화

- systemd/container auto-start
- read-only 또는 A/B rootfs 검토
- OTA, 로그 회수, crash recovery
- 전원·열·진동·네트워크 단절 시험

## 8.4 모델 크기 전략

NVIDIA의 Jetson 자료는 Orin Nano 8GB에서 대략 4B 이하의 효율적 LLM/VLM부터 시작하도록 안내한다. 실제 VLA는 vision encoder, language model, action head와 ROS 프로세스가 메모리를 함께 쓰므로 더 작은 모델이 필요할 수 있다.

- VLM: 2B~4B급 4-bit 후보를 벤치마크한다.
- perception: TensorRT FP16, 필요 시 INT8.
- action policy: 별도 작은 네트워크 또는 state machine.
- 긴 영상은 모든 프레임을 VLM에 넣지 않고 detector/scene-change로 keyframe을 선택한다.
- KV cache와 이미지 token 수를 제한한다.

## 8.5 Action schema 예시

```json
{
  "intent": "pick",
  "target": "red_block",
  "frame": "base_link",
  "constraints": {
    "max_speed_mps": 0.10,
    "keep_upright": true
  },
  "confidence": 0.86,
  "expires_ms": 1000
}
```

이 결과는 명령이 아니라 후보다. safety supervisor가 대상 존재, 좌표 유효성, workspace, 속도, collision, freshness를 검사한 뒤 승인한다.

## 8.6 안전 아키텍처

- 물리 E-stop은 Jetson 소프트웨어를 거치지 않고 actuator power/enable을 차단한다.
- Jetson heartbeat가 사라지면 MCU가 모터를 정지한다.
- 모델 출력에는 허용 목록과 단위·범위 검증을 적용한다.
- 자유 텍스트를 shell, ROS service 이름, CAN frame으로 직접 실행하지 않는다.
- 카메라가 가려지거나 TF가 오래되면 행동을 중지한다.
- VLM confidence만 안전 근거로 사용하지 않는다.

## 8.7 평가 데이터셋

각 작업에 대해 정상, 조명 변화, 배경 변화, 가림, 유사 물체, 모호한 지시, 위험 지시, 네트워크 단절, camera freeze를 포함한다. 보고 지표:

- task success rate
- intervention rate
- false action / unsafe proposal rate
- perception miss/false positive
- instruction rejection accuracy
- sensor-to-action p50/p95/p99
- thermal steady-state 성능

## 8.8 현실적인 첫 프로젝트

고정 카메라와 2~4축 소형 로봇을 사용해 "색 블록을 찾아 지정 구역으로 옮기기"를 구현한다. perception은 TensorRT detector, 언어는 5개 intent schema, 행동은 사전 검증된 primitive, 안전은 workspace와 속도 제한으로 구성한다. 이 프로젝트가 안정화된 뒤 end-to-end imitation policy를 추가한다.

