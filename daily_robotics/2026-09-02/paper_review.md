# 논문 리뷰 — Robot Operating System 2: Design, Architecture, and Uses in the Wild

## 논문 정보

- 저자: Steve Macenski, Tully Foote, Brian Gerkey, Chris Lalancette, William Woodall
- 게재: *Science Robotics*, 7(66), 2022, eabm6074
- 분야: 로봇 소프트웨어 아키텍처 / 미들웨어
- 원문: [arXiv:2211.07752](https://arxiv.org/abs/2211.07752)
- 출판본 DOI: [10.1126/scirobotics.abm6074](https://doi.org/10.1126/scirobotics.abm6074)

이 글은 새로운 필터나 플래너를 제안하는 알고리즘 논문이라기보다, ROS 1에서 ROS 2로 넘어오며 어떤 시스템 제약을 해결하려 했는지와 실제 로봇 배포 사례를 정리한 **근간 아키텍처 논문**이다. 첫 일일 학습에서 읽으면 이후 QoS, executor, security, real-time 공부가 왜 필요한지 지도를 얻을 수 있다.

## 1. 해결하려는 문제

ROS 1은 연구용 모듈을 빠르게 연결하는 데 큰 성공을 거뒀지만, 한 대의 로봇/신뢰 가능한 네트워크/비실시간 워크로드라는 암묵적 가정이 강했다. 제품 환경에서는 다음 질문에 답해야 한다.

- Wi-Fi 손실이 큰데 카메라와 제어 명령을 같은 전달 정책으로 다뤄도 되는가?
- 단일 master가 죽거나 네트워크가 분리되면 전체 graph는 어떻게 되는가?
- 여러 로봇, 임베디드 MCU, 데스크톱, 클라우드를 같은 통신 모델로 묶을 수 있는가?
- deadline과 보안이 필요한 산업/우주 시스템에서 연구 프로토타입을 어떻게 확장할 것인가?

논문의 핵심 문제의식은 “좋은 패키지가 많다”만으로 제품급 로봇 플랫폼이 되지는 않는다는 것이다. 통신 의미론, 장애 격리, 플랫폼 이식성, 보안, 수명주기와 운영 사례까지 아키텍처에 포함해야 한다.

## 2. 핵심 아이디어 및 수학적 직관

### DDS 위의 얇은 계층과 분산 discovery

ROS 2는 검증된 산업용 publish/subscribe 표준인 DDS를 미들웨어 기반으로 사용하고, `rmw` 추상화로 특정 DDS 구현에 종속되지 않도록 한다. ROS graph의 node들은 중앙 master 하나에 의존하기보다 분산 discovery로 서로를 찾는다. 장점은 단일 장애점 감소와 구현 선택권이고, 비용은 discovery 트래픽과 벤더별 세부 동작 차이다.

### “한 가지 통신 정책” 대신 QoS 벡터

통신 품질은 하나의 숫자가 아니라 다음과 같은 정책 벡터로 볼 수 있다.

```text
QoS = (reliability, history/depth, durability, deadline, lifespan, liveliness, ...)
```

카메라 프레임은 일부 손실을 허용하고 최신성을 택할 수 있지만, 저주기 상태 전이나 명령은 reliable이 더 알맞을 수 있다. 중요한 직관은 **더 강한 QoS가 언제나 더 좋은 것이 아니라 데이터의 시간 가치에 맞아야 한다**는 점이다. 또한 publisher가 제공하는 정책과 subscriber가 요구하는 정책이 호환되어야 실제 연결이 성립한다.

### 실행 정책과 데이터 전달의 분리

DDS가 데이터를 “언제 준비 상태로 만들지” 담당한다면 executor는 준비된 subscription, timer, service callback 중 “무엇을 어느 스레드에서 실행할지” 정한다. 이를 단순화하면 전체 반응 시간은 다음 항들의 합이다.

```text
T_response = T_transport + T_wait_in_executor + T_callback + T_interference
```

네트워크 평균 지연만 줄여도 executor 대기나 다른 callback 간섭의 최악값이 크면 deadline을 보장할 수 없다. 오늘 코드가 callback group을 두 개로 나누고 SPSC 큐를 경계에 둔 이유도 `T_callback`과 `T_interference`를 관찰·제한하기 위해서다.

### 생태계와 실제 배포 사례

논문은 지상·해양·항공·우주·산업 사례를 통해 공통 인터페이스와 패키지 재사용이 개발 시간을 줄인다고 설명한다. 엔지니어 관점에서 중요한 것은 “ROS 2를 사용했다”보다도, 팀 간 경계를 메시지 계약으로 만들고 하드웨어별 구성 요소를 교체 가능하게 했다는 점이다.

## 3. 실무 적용 가능성 및 한계점

### 적용하기 좋은 지점

- 센서, 인식, 상태 추정, 계획, 제어를 node/topic/action 경계로 분리해 팀 소유권을 명확히 할 때
- 센서에는 best-effort, 상태/명령에는 reliable처럼 데이터 의미에 따라 QoS를 설계할 때
- `rmw` 경계를 유지해 DDS 구현 또는 하드웨어 플랫폼을 교체할 가능성이 있을 때
- lifecycle, launch, diagnostics, tracing을 함께 써서 “실행되는 데모”를 “운영 가능한 시스템”으로 올릴 때

### 한계와 주의점

1. **ROS 2 사용 자체가 hard RT 인증이 아니다.** PREEMPT_RT, thread priority/affinity, page fault 억제, bounded allocation, WCET 측정이 별도로 필요하다.
2. **DDS 추상화는 완전한 동작 동일성을 뜻하지 않는다.** loaned message, discovery, shared-memory transport, resource limit 지원은 RMW/DDS 구현과 버전에 따라 달라질 수 있다.
3. **분산 discovery는 운영 비용을 옮긴다.** 중앙 master 장애는 줄지만 대규모 graph의 discovery 폭주, 네트워크 격리, 도메인/보안 설정을 다뤄야 한다.
4. **사례 연구는 설계 유효성의 정량 비교 실험이 아니다.** 성공 사례는 강한 실무 증거지만, 모든 아키텍처 선택의 최악 지연·비용을 통제군과 비교해 증명하지는 않는다.
5. **메시지 경계가 잘못되면 모듈성도 비용이 된다.** 지나치게 잘게 쪼갠 node, 큰 메시지의 반복 직렬화, 부적절한 QoS는 복잡성과 jitter를 키운다.

## 오늘 코드에 적용한 결정

- `/sensors/noisy_odometry`: 지연된 과거 센서보다 최신 샘플이 중요하므로 `SensorDataQoS`.
- 수신 콜백: 계산을 길게 수행하지 않고 고정 크기 값 객체를 SPSC 큐에 복사.
- 필터 콜백: 별도 callback group과 executor worker에서 predict/correct 수행.
- 큐 포화: block하지 않고 drop 카운트 증가. 실제 제품에서는 이 카운트를 diagnostics로 노출.
- 필터 내부: 동적 컨테이너 대신 고정 개수 `double`을 사용해 계산량과 메모리 사용을 bounded하게 유지.

## 읽고 나서 답해 볼 질문

1. 모터 정지 명령과 30 Hz 카메라 영상에 같은 QoS를 적용하면 어느 쪽 요구가 깨지는가?
2. 평균 callback 지연이 100 µs여도 최악 지연이 20 ms라면 1 kHz 제어에 쓸 수 있는가?
3. node를 process로 분리할 때 얻는 장애 격리와 잃는 통신 비용을 어떻게 측정할 것인가?
4. 현재 SPSC 큐에 두 개의 센서 subscription을 직접 넣으면 왜 “single producer” 가정이 깨질 수 있는가?

## 한 줄 결론

ROS 2의 가치는 DDS를 쓴다는 사실 하나가 아니라, **데이터 전달 계약(QoS), 실행 정책(executor/callback group), 플랫폼 추상화(rmw), 재사용 생태계**를 함께 설계할 수 있게 한 데 있다. 다만 제품급 RT 성능은 이 토대 위에서 개발자가 측정하고 경계를 증명해야 한다.
