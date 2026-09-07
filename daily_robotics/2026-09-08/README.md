# 2026-09-08 — 차동구동·우선순위 Executor·Monte Carlo Localization

오늘은 **차체 속도를 좌우 모터 속도로 바꾸고**, **100 Hz 제어 callback을 ROS 통신 callback과 다른 executor/thread에 격리하며**, **고정 200개 입자와 Lidar 거리로 전역 위치를 찾는다**. 핵심은 `속도 명령 → RT 지향 모터 루프 → 엔코더 → 확률적 위치추정`의 경계마다 단위, 실행 문맥, 데이터 일관성을 명시하는 것이다.

## 학습 순서 (Reading Order)

1. 아래 Mermaid 구조도에서 Topic 경계와 두 executor의 역할을 먼저 본다.
2. [`src/sensor_simulator.cpp`](src/sensor_simulator.cpp)에서 `Twist`, `LaserScan`, unicycle ground truth를 읽는다.
3. [`src/priority_motor_controller.cpp`](src/priority_motor_controller.cpp)에서 차동구동 역운동학, callback group, `add_callback_group`, atomic snapshot을 연결한다.
4. [`src/particle_localizer.cpp`](src/particle_localizer.cpp)에서 encoder 순운동학, particle predict, Lidar likelihood, `N_eff`, systematic resampling을 따라간다.
5. [`launch/daily_demo.launch.py`](launch/daily_demo.launch.py)로 세 프로세스를 실행하고 `/wheel_states`, `/mcl_pose`를 관찰한다.
6. [`paper_review.md`](paper_review.md)에서 1999 MCL 근간 논문의 문제, 확률적 직관, 제품 한계를 읽는다.
7. 누적 노트의 실차 보정·RT 검증·파티클 퇴화 체크리스트를 복습한다.

## 오늘의 세 축

| 영역 | 핵심 질문 | 오늘의 답 |
|---|---|---|
| 기초 실무 | `(v, ω)`를 좌·우 바퀴 속도로 어떻게 바꾸는가? | 반지름 `r`, 바퀴 간격 `L`을 사용해 `ωL=(v-ωL/2)/r`, `ωR=(v+ωL/2)/r`로 변환한다. |
| 심화·RT | DDS 처리 지연이 100 Hz 모터 loop를 막지 않게 하려면? | 제어/I/O callback group을 각각 전용 `SingleThreadedExecutor`에 넣고 atomic snapshot으로만 연결한다. |
| 알고리즘 | 초기 위치를 모를 때 scan으로 어떻게 여러 자세 가설을 유지하는가? | 고정 200개 particle에 motion/measurement model을 적용하고 `N_eff`가 낮을 때 systematic resampling한다. |

## 시스템 아키텍처

```mermaid
graph LR
    SIM[SensorSimulator<br/>20 Hz]
    CMD[/cmd_vel<br/>Twist]
    SCAN[/scan<br/>LaserScan]

    subgraph MOTOR[PriorityMotorController 프로세스]
        IO[일반 I/O Executor<br/>command + telemetry]
        CB[(Atomic command<br/>snapshot)]
        CTRL[제어 Executor thread<br/>100 Hz / FIFO 요청]
        WB[(Atomic wheel<br/>snapshot)]

        IO -->|단일 writer| CB
        CB -->|lock-free read| CTRL
        CTRL -->|차동구동 적분| WB
        WB -->|일관된 read| IO
    end

    WHEEL[/wheel_states<br/>JointState]
    MCL[ParticleLocalizer<br/>200 particles]
    POSE[/mcl_pose<br/>PoseWithCovarianceStamped]

    SIM --> CMD --> IO
    IO --> WHEEL -->|motion model| MCL
    SIM --> SCAN -->|measurement model| MCL
    MCL --> POSE
```

브라우저에서 검색·테마·경로 강조가 가능한 검증된 확장 구조도는 [`architecture.html`](architecture.html)에 생성한다. 다이어그램 본문은 한국어이며 Archify 뷰어의 고정 UI는 지원 로케일 제약으로 영어 fallback을 사용한다.

## 1. 차동구동: body 명령과 wheel 명령

로봇 중심의 선속도 `v`, yaw 각속도 `ω`, 바퀴 간격 `L`, 반지름 `r`에 대해 좌·우 바퀴 각속도는 다음이다.

```text
v_L = v - ωL/2                 ω_L = v_L/r
v_R = v + ωL/2                 ω_R = v_R/r
```

코드의 `control_tick()`이 바로 이 식을 계산한다. 반대로 엔코더 회전각 변화 `ΔφL`, `ΔφR`를 받으면 다음 순운동학으로 차체 이동을 얻는다.

```text
Δs_L = r ΔφL
Δs_R = r ΔφR
Δs   = (Δs_R + Δs_L)/2
Δθ   = (Δs_R - Δs_L)/L

x' = x + Δs cos(θ + Δθ/2)
y' = y + Δs sin(θ + Δθ/2)
θ' = wrap(θ + Δθ)
```

`particle_localizer.cpp`는 각 particle에 이 식과 wheel slip을 나타내는 Gaussian noise를 적용한다. `r`이 틀리면 직진 거리 scale이 틀어지고, `L`이 틀리면 회전량 scale이 틀어진다. 실차에서는 직진/제자리 회전 실험으로 두 값을 따로 보정해야 한다.

## 2. Callback group과 우선순위 격리

한 `MultiThreadedExecutor`에 callback을 모두 넣으면 병렬성은 생기지만 제어 callback의 실행 순서나 OS thread 우선순위를 직접 보장하지 못한다. 오늘 코드는 한 Node 내부에 두 `MutuallyExclusive` group을 만들고 각각 다른 executor에 명시적으로 등록한다.

```text
main / 일반 우선순위
  io_executor.spin()
    ├─ /cmd_vel subscription
    └─ 50 Hz JointState publish + log

control thread / SCHED_FIFO 60 요청
  control_executor.spin()
    └─ 100 Hz motor math + atomic read/write만 수행
```

`add_callback_group(group, node_base_interface)`는 Node 전체가 아니라 선택한 callback group만 executor에 넣는다. 이 분리는 DDS serialization, console log, 동적 message 처리 시간을 제어 thread 밖으로 보낸다.

단, 이것만으로 hard real-time이 되지는 않는다.

- 일반 Docker에서는 `pthread_setschedparam(..., SCHED_FIFO, ...)`가 `EPERM`으로 실패할 수 있다.
- 성공하려면 RT 권한, CPU affinity/isolation, IRQ 배치, page fault 방지, WCET 측정이 함께 필요하다.
- `SingleThreadedExecutor` 자체의 wait-set scheduling과 RMW 경로도 latency 분석 대상이다.
- 장시간 blocking callback이나 priority inversion이 있는 lock을 어느 경로에도 넣지 않아야 한다.

## 3. Atomic snapshot의 일관성

명령은 `linear`, `angular`, `received_at` 세 필드가 한 묶음이어야 한다. 각각을 독립 atomic으로 읽기만 하면 서로 다른 command callback에서 온 값이 섞일 수 있다. 이 예제는 다음 sequence protocol을 쓴다.

```text
writer: sequence를 홀수 → payload atomic 저장 → sequence를 짝수
reader: 앞 sequence 읽기 → payload 읽기 → 뒤 sequence 읽기
        두 sequence가 같고 짝수일 때만 snapshot 채택, 아니면 재시도
```

payload 자체도 `std::atomic<uint64_t>`이므로 plain-memory seqlock에서 생길 수 있는 C++ data race를 피한다. `double`은 `memcpy`로 64-bit 비트열에 옮긴다. 이 설계는 **단일 writer**를 전제로 하며, `is_lock_free()` 결과는 CPU/표준 라이브러리 구현마다 다를 수 있다. 오늘 x86-64 Jazzy container에서는 `true`였다.

## 4. Monte Carlo Localization 수식과 코드

Bayes filter의 두 단계는 다음이다.

```text
prediction:  p(x_t | z_1:t-1, u_1:t)
             = ∫ p(x_t | u_t, x_t-1) bel(x_t-1) dx_t-1

correction:  bel(x_t) ∝ p(z_t | x_t) · prediction
```

MCL은 연속 분포를 `particle_i=(x_i,y_i,yaw_i,weight_i)` 집합으로 근사한다.

1. **초기화:** 200개 자세를 알려진 직사각형 방 전체에 균일 분포시킨다.
2. **Predict:** encoder `Δs, Δθ`와 motion noise로 모든 particle을 이동시킨다.
3. **Correct:** 각 particle에서 16개 Lidar 광선을 벽까지 ray-cast하고 잔차를 Gaussian likelihood로 바꾼다.
4. **Normalize:** 작은 확률 곱의 underflow를 줄이기 위해 log weight에서 최댓값을 뺀 뒤 `exp`한다.
5. **Resample:** `N_eff=1/Σw_i² < 100`이면 systematic resampling으로 유력한 가설을 복제한다.
6. **Estimate:** `x,y`는 가중 평균, yaw는 `atan2(Σw sinθ, Σw cosθ)` 원형 평균으로 게시한다.

고정 `std::array<Particle, 200>`은 메모리 상한을 명시한다. 하지만 particle 수가 충분하다는 뜻은 아니다. 복잡한 지도, kidnapped robot, 대칭 환경에서는 입자 수 적응, random injection, 더 강한 sensor model이 필요하다.

## 빌드와 실행

ROS 2 Jazzy workspace의 `src` 아래에 이 폴더를 둔다.

```bash
cd ~/ros2_ws
colcon build --packages-select daily_robotics_2026_09_08 --event-handlers console_direct+
source install/setup.bash
ros2 launch daily_robotics_2026_09_08 daily_demo.launch.py
```

다른 터미널에서 노드와 데이터를 확인한다.

```bash
source ~/ros2_ws/install/setup.bash
ros2 node list
ros2 topic echo /wheel_states --once
ros2 topic echo /mcl_pose --once
ros2 topic hz /wheel_states
```

예상 관찰:

1. `/sensor_simulator`, `/priority_motor_controller`, `/particle_localizer`가 보인다.
2. 시작 시 `atomic command path lock-free=true` 또는 플랫폼에 따른 `false`가 명시된다.
3. RT 권한이 없으면 `SCHED_FIFO ... unavailable` 경고가 나오지만 callback group 격리와 기능은 유지된다.
4. 좌우 wheel position 차이가 곡률을 만들고, MCL 분산과 pose가 초기 전역 가설에서 실제 경로 근처로 모인다.

## 검증 기록 (2026-09-08, ROS 2 Jazzy)

- 공식 `ros:jazzy-ros-base`, GCC 13.3에서 `colcon build --packages-select daily_robotics_2026_09_08` 성공
- `-Wall -Wextra -Wpedantic` 컴파일 경고 없음, 세 실행 파일과 launch 설치 확인
- 통합 launch에서 세 Node 발견, `/wheel_states`와 `/mcl_pose` 각각 1회 수신 성공
- atomic command path는 실행 플랫폼에서 `lock-free=true`
- 권한 없는 container에서 `SCHED_FIFO priority 60`은 error 1로 실패하고 일반 scheduling으로 안전하게 fallback
- 예시 encoder: `left=2.366 rad`, `right=2.989 rad`; 양의 yaw 명령과 일치하는 우측 회전량 증가 확인
- 약 14초 구간 말미에 ground truth `(0.50, 1.15, 0.25)` 부근에서 MCL `(0.61, 1.07, 0.24)`를 관찰해 초기 전역 분포가 실제 궤적 근처로 수렴함을 확인
- Archify showcase artifact 검사 9/9, 오류 0, 경고 0
- 구조도 specification SHA-256 `d7153a7531b21ca6535697896274cee7e73ef67f8f333efadcf6cf52b7b8cfe1`
- 구조도 HTML SHA-256 `cd25d839d01a8e1af428e3e3ff3e086ef2877817c2f10928a71d6c87729e050d`
- Chrome 자동 검증에서 1440×900, 1600×1000, 1920×1080, 2048×1320 모두 overflow·가독성 문제 없이 통과
- 1440×900 light와 2048×1320 dark 캡처를 직접 확인해 연결선, label, process boundary, card, 양쪽 theme 가독성 통과

검증 container의 `/ws/build`, `/ws/install`, `/ws/log`는 임시 파일시스템에만 생성했고 저장소에는 남기지 않았다.

## 실패를 의도적으로 읽는 법

- wheel 방향이 반대면 `JointState.name`과 모터 driver sign convention을 먼저 확인한다.
- 직진 거리만 비례 오차가 나면 `wheel_radius`, 회전만 비례 오차가 나면 `wheel_separation`을 보정한다.
- `atomic ... lock-free=false`이면 해당 플랫폼에서 이 경로를 RT 안전하다고 주장하지 말고 검증된 RT buffer를 사용한다.
- `SCHED_FIFO`가 실패하면 capability/ulimit만 보지 말고 runaway RT thread를 막는 watchdog도 함께 설계한다.
- MCL이 잘못된 대칭 위치에 수렴하면 beam 수, 지도 특징, 초기 particle 수, random injection을 점검한다.
- `N_eff`가 매 scan마다 매우 작으면 sensor sigma가 과도하게 작거나 map/scan frame이 어긋났을 수 있다.
- pose 평균은 다봉 분포에서 실제로 가능하지 않은 중간 자세가 될 수 있으므로 제품 UI에는 cluster/mode도 노출한다.

## 파일 지도

```text
daily_robotics/2026-09-08/
├── CMakeLists.txt
├── package.xml
├── README.md
├── paper_review.md
├── architecture.archify.json
├── architecture.html
├── launch/
│   └── daily_demo.launch.py
└── src/
    ├── sensor_simulator.cpp
    ├── priority_motor_controller.cpp
    └── particle_localizer.cpp
```

## 누적 지식 연결

- [`../../knowledge/kinematics/differential_drive.md`](../../knowledge/kinematics/differential_drive.md)
- [`../../knowledge/realtime/callback_group_priority_isolation.md`](../../knowledge/realtime/callback_group_priority_isolation.md)
- [`../../knowledge/localization/particle_filter.md`](../../knowledge/localization/particle_filter.md)

## 공식·원문 레퍼런스

- [ROS 2 Jazzy: Using Callback Groups](https://docs.ros.org/en/jazzy/How-To-Guides/Using-callback-groups.html)
- [ROS 2: Executors](https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Executors.html)
- [ROS 2 Control Jazzy: diff_drive_controller](https://control.ros.org/jazzy/doc/ros2_controllers/diff_drive_controller/doc/userdoc.html)
- [CMU Robotics Institute: Monte Carlo Localization publication](https://publications.ri.cmu.edu/monte-carlo-localization-efficient-position-estimation-for-mobile-robots)
- [Fox et al. 1999 paper PDF](https://www.cs.cmu.edu/~thrun/papers/fox.aaai99.pdf)
