# micro-ROS Executor와 자원 예산

## 핵심 구분

- **실행 순서:** rclc Executor에 handle을 등록한 순서와 trigger 조건으로 callback 체인을 정의한다.
- **데이터 의미:** LET는 주기 시작에 입력을 취해 로컬 복사로 처리하는 일관된 스냅샷 모델을 제공한다.
- **CPU 예산:** RTOS sporadic/reservation scheduler는 `budget B`와 `period T`로 정상 우선순위 CPU 사용을 제한한다.
- **전송 예산:** 고정 IDL, MTU, stream reliability/history를 함께 봐야 RAM·대역폭 상한을 정할 수 있다.

## 설계 체크리스트

1. 런타임에 handle이나 메시지 크기가 늘어나지 않는가?
2. callback별 WCET, period, deadline, blocking 시간을 측정했는가?
3. `sum(B_i/T_i)`뿐 아니라 높은 우선순위 간섭과 middleware mutex를 분석했는가?
4. best-effort 메시지가 MTU 이내인가? reliable fragmentation history가 RAM에 들어가는가?
5. 평균 latency가 아니라 overload와 재전송을 포함한 worst-case trace를 남겼는가?

## 기억할 한계

고정 배열과 예산 계산은 결정성의 재료이지 hard real-time 보증 자체가 아니다. CPU scheduler, interrupt, DMA, allocator, middleware lock, transport 재전송을 합친 end-to-end 검증이 필요하다.

## 참고

- https://github.com/micro-ROS/micro-ROS.github.io/blob/master/_docs/concepts/client_library/execution_management/index.md
- https://github.com/eProsima/Micro-XRCE-DDS-Docs/blob/master/docs/client.rst
- https://arxiv.org/abs/2105.05590
