# 계산 상한이 있는 온라인 센서 보정

Online calibration은 sensor callback에서 최적화를 직접 실행하지 않는다. 입력 수신과 solver를 분리해야 callback starvation과 데이터 age 증가를 피할 수 있다.

## 권장 경계

1. callback은 필요한 scalar/고정크기 값만 bounded ring에 O(1) 복사한다.
2. worker는 주기마다 ring snapshot을 취한다.
3. window, 후보 수, iteration, correspondence 수에 상한을 둔다.
4. 결과에는 residual, 사용 표본 수, 경계해 여부, 실행 시간을 함께 발행한다.
5. 독립 supervisor가 품질과 deadline을 연속 여러 회 확인한 뒤 적용한다.

Grid search 시간 오프셋 보정의 대략적 복잡도는 후보 수 `C`, 저속 센서 표본 수 `M`, 고속 표본 binary search `log N`에 대해 `O(C M log N)`이다. `C`, `M`, `N`을 고정하면 데이터 의존 무한 반복은 없지만, 다음까지 자동으로 hard RT가 되는 것은 아니다.

- middleware serialization과 allocator
- page fault/cache/TLB miss
- executor 및 OS scheduler wake-up latency
- logging과 filesystem I/O

따라서 `bounded algorithm`과 `measured WCET`, `hard real-time guarantee`를 구분해 문서화한다. 추정값이 search boundary에 붙거나 excitation이 부족하면 새 값을 적용하지 않고 마지막 정상값 또는 factory calibration으로 fallback한다.
