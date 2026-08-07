# 05. 실제 로봇 테스트 파이프라인

[실제 로봇 테스트](../glossary/README.md#real-robot-test)는 시뮬레이션과 다릅니다. 실제 로봇에는 사람, 전원, 통신 지연, 바퀴 미끄러짐, 센서 노이즈, 물리적 손상 위험이 있습니다.

## 실제 테스트 단계

| 단계 | 이름 | 목적 | 통과 조건 |
|---:|---|---|---|
| 0 | desk check | 코드/설정/모델 파일 확인 | manifest 완성 |
| 1 | dry-run | 모터 없이 센서와 추론만 실행 | 위험 명령 0개 |
| 2 | stand test | 바퀴를 띄우고 명령 확인 | 방향/속도 정상 |
| 3 | tethered low-speed | 줄 또는 제한 공간에서 저속 주행 | emergency stop 확인 |
| 4 | supervised run | 사람이 가까이서 감독 | collision 없음 |
| 5 | mission run | 실제 작업 시나리오 | success/failure 기록 |
| 6 | regression | 같은 조건 반복 | 결과 편차 분석 |

## 실제 테스트 전 필수 체크

- [ ] emergency stop이 물리적으로 동작한다.
- [ ] `/cmd_vel` 최대 속도가 parameter로 제한되어 있다.
- [ ] [safety filter](../glossary/README.md#safety-filter)가 최종 명령 앞에 있다.
- [ ] [dry-run](../glossary/README.md#dry-run) 모드가 있다.
- [ ] 모델 파일, config, launch 파일이 버전 관리된다.
- [ ] [rollback](../glossary/README.md#rollback) 버전이 준비되어 있다.
- [ ] [rosbag2](../glossary/README.md#rosbag2) 기록 topic 목록이 정해져 있다.

## Sim-to-Real gap 대응

| gap | 실제 증상 | 대응 |
|---|---|---|
| 센서 노이즈 | 모델이 벽/사람을 잘못 인식 | augmentation, real data fine-tuning |
| 지연 | 늦게 회전하거나 늦게 정지 | latency budget, C++ 추론, TensorRT |
| 동역학 차이 | 시뮬레이터보다 더 미끄러짐 | controller tuning, system ID |
| 조명/질감 차이 | camera policy 실패 | domain randomization |
| TF/calibration | 장애물 위치가 어긋남 | calibration, frame check |

## 실제 로봇 로그 설계

```text
/scan
/camera/image_raw
/odom
/tf
/policy/cmd_vel_raw
/cmd_vel
/diagnostics
/robot_state
```

정책 출력인 `/policy/cmd_vel_raw`와 최종 명령인 `/cmd_vel`을 둘 다 기록해야 안전 필터가 실제로 무엇을 막았는지 알 수 있습니다.

## 통과 기준

- 시뮬레이션 성공이 실제 성공을 보장하지 않는 이유를 설명한다.
- 저속 제한과 emergency stop 없이는 실제 로봇 테스트를 시작하지 않는다.
- 실패가 생기면 모델, config, bag, commit을 함께 기록한다.
