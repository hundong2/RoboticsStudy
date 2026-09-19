# 논문 리뷰 — Budget-based real-time Executor for Micro-ROS

## 서지 정보

- Jan Staschulat, Ralph Lange, Dakshina Narahari Dasari, 2021
- arXiv:2105.05590 [cs.RO], *Budget-based real-time Executor for Micro-ROS*
- 원문: https://arxiv.org/abs/2105.05590

## 1. 해결하려는 문제

ROS 2 Executor는 준비된 콜백을 찾아 실행하지만, “이 콜백은 매 100 ms마다 최대 30 ms만 CPU를 써야 한다” 같은 OS 스케줄링 예산을 직접 표현하지 못한다. MCU에서는 센서 처리, 안전 콜백, 진단 작업이 같은 CPU를 공유하므로 한 콜백의 과도한 실행이 다른 제어 체인의 deadline miss로 번질 수 있다.

논문은 micro-ROS rclc Executor와 NuttX의 sporadic scheduling을 연결해 각 콜백 worker에 실행 예산과 보충 주기를 주는 방법을 제안한다. 목표는 단순한 평균 지연 개선이 아니라, 서로 다른 소프트웨어 컴포넌트 사이의 **시간 간섭을 제한**하는 것이다.

## 2. 핵심 아이디어와 수학적 직관

태스크 `tau_i`에 주기 `T_i`, 정상 우선순위에서 사용할 수 있는 예산 `B_i`를 준다. 태스크가 시각 `t_x`부터 `b`만큼 CPU를 소비하면 그 양은 `t_x + T_i`에 보충된다. 한 주기 안에 `B_i`를 모두 쓰면 낮은 우선순위로 내려간다.

```text
정상 우선순위에서 허용되는 평균 CPU 몫 ≈ B_i / T_i
예: B=30 ms, T=100 ms → 정상 우선순위 예산 30%
```

Executor thread는 DDS/XRCE wait-set에서 준비된 입력을 확인하고, 해당 worker가 READY일 때만 `rcl_take`로 메시지를 넘긴다. worker는 미리 설정한 스케줄링 정책으로 콜백을 실행한다. 이 분리는 다음 두 층을 연결한다.

1. ROS 층: 어떤 subscription callback이 실행 준비가 되었는가?
2. RTOS 층: 그 callback이 언제, 어느 우선순위와 예산으로 CPU를 사용할 수 있는가?

논문의 ping-pong 실험에서는 STM32F407/NuttX에서 sporadic budget이 high-priority 처리량을 실제로 제한하고, 남는 CPU를 low-priority 작업이 사용할 수 있음을 보였다. “예산 초과 시 완전 정지”가 아니라 낮은 우선순위로 계속 실행할 수 있는 work-conserving 성질도 확인했다.

## 3. 실무 적용 가능성과 한계

### 적용하기 좋은 곳

- 모터 제어와 상태 진단이 같은 MCU를 공유하지만 진단이 제어를 방해하면 안 되는 경우
- 여러 센서 callback의 CPU 사용량을 컴포넌트별로 격리해야 하는 경우
- 제품 통합자가 `budget/period` 형태로 실행 자원을 검토하고 admission test를 만들고 싶은 경우

### 그대로 믿으면 안 되는 부분

- 논문 실험은 특정 STM32F407와 NuttX 구현에 대한 prototype이다. 다른 RTOS/보드에서도 같은 보장이 자동으로 생기지 않는다.
- 논문 당시 middleware 접근은 단일 스레드이며 mutex가 필요해, worker를 늘려도 middleware 병목과 우선순위 역전 가능성이 남는다.
- 예산은 “최대 사용량 제한”에 가깝다. 모든 태스크가 자신에게 배정된 CPU를 반드시 얻는다는 충분조건은 아니며, end-to-end deadline 증명에는 WCET, blocking, 통신 지연 분석이 추가로 필요하다.
- throughput 그래프만으로 hard real-time을 선언할 수 없다. 긴 시간의 worst-case trace, overload/fault injection, RTOS 설정 검증이 필요하다.

## 오늘 실습과의 연결

오늘 코드는 논문의 sporadic scheduler를 구현하지 않는다. 대신 그보다 앞선 설계 질문 두 가지를 연습한다.

1. callback 한 번의 입력과 계산량을 고정할 수 있는가? — 64셀 고정 배열과 8×8 고정 반복.
2. 통신 한 번의 자원 상한을 설명할 수 있는가? — payload, MTU, 조각 수, wire bytes를 명시.

이 두 상한이 있어야 이후 MCU에서 `B_i`를 WCET 측정과 연결하고, `T_i`를 센서 주기/deadline과 연결할 수 있다. 다음 단계는 실제 보드에서 실행 시간을 trace하고 `B/T` admission 조건을 세우는 것이다.
