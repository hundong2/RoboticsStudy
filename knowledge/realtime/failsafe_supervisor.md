# Deterministic Fail-safe Supervisor

## 핵심 분리

안전 감독기는 최소 세 경로를 구분해야 한다.

1. **데이터 ingest:** 최신 명령·측정을 bounded copy로 받는다.
2. **고주기 판단:** 고정 크기 계산, sample-age watchdog, 상태 전이, 제한 명령 생성.
3. **저주기 관측:** 로그·문자열·DiagnosticArray·파일/네트워크 기록.

2번 경로의 반복 횟수와 메모리 사용량을 입력 크기와 무관하게 제한한다. 3번이 지연되거나 실패해도 2번이 멈추지 않게 한다.

## 통신 안전 계약

```text
Deadline          = 샘플 간격 계약
Liveliness lease  = Publisher 생존 주장 계약
sample age        = 소비자가 실제로 가진 최신 데이터의 나이
```

셋은 서로 대체재가 아니다. RMW가 어떤 QoS 사건을 지원하는지 확인하고, 최종 안전 판단에는 monotonic clock 기반 local watchdog을 함께 둔다.

## 상태 머신 원칙

- 위험 전이는 빠르고 보수적으로 한다.
- 복구 전이는 더 긴 healthy dwell time을 요구한다.
- `SUSPECT`와 `RECOVERING`을 두어 확률 threshold 부근 채터링을 막는다.
- SAFE_STOP 출력은 상태 문자열이 아니라 실제 actuator command에서 검증한다.
- 프로세스 crash나 bus failure에도 멈추도록 최종 하드웨어 차단 경로를 독립시킨다.

고정 배열과 짧은 callback만으로 hard RT가 되지는 않는다. scheduler priority, CPU affinity, page locking, allocator, DDS/RMW, kernel trace, WCET와 priority inversion까지 측정해야 한다.
