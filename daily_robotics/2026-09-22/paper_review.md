# 논문 리뷰 — IMM 기반 쿼드로터 액추에이터·센서 고장 진단

## 서지 정보

- Yujiang Zhong, Youmin Zhang, Wei Zhang, Hao Zhan
- **Actuator and Sensor Fault Detection and Diagnosis for Unmanned Quadrotor Helicopters**
- IFAC-PapersOnLine 51(24), 998–1003, 2018
- DOI: [10.1016/j.ifacol.2018.09.708](https://doi.org/10.1016/j.ifacol.2018.09.708)

오늘 논문은 최신 논문 대신 **모델 기반 fault detection과 IMM을 실제 로봇 액추에이터 문제에 연결하는 근간 사례**로 골랐다. 쿼드로터라는 대상은 다르지만, “정상/고장 모델을 병렬로 두고 관측과 더 잘 맞는 모델의 확률을 높인다”는 원리는 오늘 단일 관절 실습과 직접 연결된다.

## 1. 해결하려는 문제

비행 로봇에서 모터 출력이 일부 줄거나 센서에 bias가 생기면 제어기는 잘못된 상태를 정상으로 믿을 수 있다. 단순 threshold는 급기동, 외란, 잡음까지 고장으로 오인하기 쉽고, 하나의 observer는 어떤 고장 종류인지 구분하기 어렵다.

논문은 두 부류를 함께 다룬다.

- **액추에이터 고장:** 모터가 명령의 일부만 내는 loss of control effectiveness
- **센서 고장:** 측정값에 일정한 bias가 더해지는 현상

핵심 질문은 “불확실한 동역학과 측정 잡음 속에서도 어떤 고장 모드가 현재 관측을 가장 잘 설명하는가?”다.

## 2. 핵심 아이디어와 수학적 직관

IMM은 각 고장 가설마다 필터를 하나씩 둔다. 필터 `j`는 자신의 동역학으로 다음 상태를 예측하고 residual을 만든다.

```text
x_j^- = f_j(x, u)
r_j   = z - h_j(x_j^-)
Lambda_j ∝ exp(-0.5 r_j^T S_j^-1 r_j) / sqrt(det S_j)
mu_j  = Lambda_j c_j / sum_l Lambda_l c_l
```

`Lambda_j`는 모델 `j`의 예측 오차가 그 필터가 예상한 공분산 안에서 얼마나 자연스러운지 나타내는 likelihood다. residual이 작고 불확실성으로도 설명 가능하면 해당 모드 확률 `mu_j`가 올라간다. 모드 전이행렬은 “정상에서 갑자기 고장”, “고장에서 수리/복구”가 얼마나 자주 일어나는지를 prior로 표현한다.

문제는 모터 4개×센서 여러 개×각 고장 조합을 모두 별도 모델로 만들면 필터 수가 조합적으로 늘어난다는 점이다. 논문은 **state augmentation(SA)**으로 고장 크기를 상태처럼 추정해 elemental filter 수를 줄이고, 단순 고장 존재 여부뿐 아니라 magnitude까지 식별하려 한다. 실무적으로는 “모든 조합을 enum으로 나열하지 말고, 공통 동역학과 연속 fault parameter를 분리하라”는 설계 교훈이다.

오늘 코드는 이 아이디어의 가장 작은 뼈대다.

```text
healthy model:  g=1.00
degraded model: g=0.25
v(k+1)=a v(k)+(1-a)g u(k)+w(k)
```

상태 증강까지 구현하지 않고 두 개 scalar Kalman filter만 사용하므로, 논문의 다중 모터·센서 진단을 재현한다고 주장하지 않는다.

## 3. 실무 적용 가능성

### 유용한 지점

1. **설명 가능성:** 신경망 score가 아니라 물리 모델별 residual·공분산·확률을 남길 수 있다.
2. **안전 상태 머신과 결합:** `p(fault)>threshold`만으로 즉시 토글하지 않고 지속 시간, hysteresis, recovery gate를 설계할 수 있다.
3. **다중 센서 확장:** encoder, motor current, IMU, thrust estimate를 한 상태에 넣으면 관측 가능성이 좋아진다.
4. **계산 상한:** 모델 수와 상태 차원을 고정하면 한 주기의 행렬 크기와 반복 횟수를 미리 제한할 수 있다.

### 제품 적용 순서

```text
실제 healthy/fault 데이터 수집
→ 모델·Q/R·전이확률 식별
→ detection/recovery threshold를 비용 기반으로 설정
→ fault injection + rosbag replay
→ end-to-end latency/WCET 측정
→ 독립 안전 채널과 연결
```

IMM은 진단기이지 안전장치 자체가 아니다. 소프트웨어가 0 명령을 publish해도 드라이버, 버스, 인버터가 멈추지 않으면 안전 정지가 아니다. 제품에서는 STO, watchdog relay, safety MCU처럼 독립적인 최종 차단 경로가 필요하다.

## 4. 한계와 비판적 읽기

- 논문 검증은 두 simulation scenario 중심이므로 실제 진동, 온도, 전압 강하, propeller damage에 대한 일반화는 별도 실험이 필요하다.
- 모델에 없는 외란은 고장 모델의 likelihood를 높일 수 있다. 예를 들어 급격한 부하 토크가 loss-of-effectiveness처럼 보일 수 있다.
- 상태 증강은 모델 수를 줄이지만 추정 차원을 늘리고, fault parameter와 외란이 관측상 구분되지 않으면 식별성이 나빠진다.
- 평균 계산 시간이 충분히 짧다는 것과 deadline을 절대 넘지 않는다는 것은 다르다. 필터 실행시간뿐 아니라 센서 timestamp, DDS queue, executor scheduling, 출력 전달을 함께 측정해야 한다.
- 고장 확률 threshold에는 false positive와 missed detection의 비용이 들어간다. 논문 수치를 다른 모터에 그대로 복사하면 안 된다.

## 엔지니어가 가져갈 결론

좋은 fault detector는 “residual이 컸다”에서 끝나지 않는다. 어떤 물리 가설이 residual을 설명하는지 확률로 비교하고, 통신 건강성과 독립된 안전 상태 머신을 통해 실제 제한 명령으로 바꿔야 한다. 오늘 실습은 그 전체 연결을 작게 보여주지만, 실제 배포 전에는 모델 식별·관측 가능성·독립 차단·WCET·오경보 비용을 반드시 추가해야 한다.

## 참고 링크

- [논문 DOI](https://doi.org/10.1016/j.ifacol.2018.09.708)
- [IFAC 2018 proceedings 목차(저자·페이지 확인)](https://www.proceedings.com/content/041/041652webtoc.pdf)
- [ROS 2 Jazzy QoS/RMW 구현 참고](https://docs.ros.org/en/ros2_documentation/jazzy/Tutorials/Advanced/Creating-An-RMW-Implementation.html)
