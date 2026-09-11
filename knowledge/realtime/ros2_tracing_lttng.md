# ROS 2 tracing과 LTTng 실무 기준

## Application timer와 tracing의 차이

Callback 안에서 `steady_clock` 시작/끝을 재면 사용자 코드 실행시간을 쉽게 얻지만, 그 callback이 왜 늦게 시작됐는지는 알 수 없다. ROS 2 core tracepoint는 publish, `rcl_take`, executor dispatch, callback start/end를 같은 trace에 남겨 다음 구간을 분해한다.

1. Publisher/application 구간
2. RMW/DDS transport와 receive
3. Executor queue/wakeup/dispatch
4. Callback 실행

두 방법은 경쟁 관계가 아니다. Domain 단계가 보이는 application marker와 middleware 흐름이 보이는 trace를 같은 재현 workload에서 함께 쓴다.

## 세션 순서

`ros2_tracing`은 현재 Linux에서 LTTng를 사용한다. 초기화 metadata가 필요하므로 tracing session을 node보다 먼저 시작한다.

```bash
ros2 run tracetools status
ros2 trace start experiment_name
ros2 launch my_package workload.launch.py
ros2 trace stop experiment_name
```

Trace 저장 위치는 `ROS_TRACE_DIR`, 그다음 `ROS_HOME/tracing`, 마지막으로 기본 `~/.ros/tracing` 순서로 결정된다. 저장소의 build artifact와 섞지 말고 run ID, commit, CPU/kernel/RMW 설정을 metadata로 남긴다.

## Real-time 측정 주의점

- Tracepoint가 low-overhead여도 무비용은 아니다. On/off A/B 반복으로 perturbation을 측정한다.
- 평균보다 p99/p99.9/max, sample loss, trace buffer overflow를 본다.
- 첫 tracepoint 호출의 thread registration/one-time allocation을 initialization 단계에서 처리할지 검토한다.
- LTTng sub-buffer size/count를 event rate와 허용 메모리 예산에 맞춘다.
- Userspace trace와 kernel scheduling/IRQ trace는 요구 권한과 overhead가 다르다.
- CPU frequency, affinity, RT priority, background workload, RMW를 실험 사이에 고정한다.
- `TRACETOOLS_RUNTIME_DISABLE=1` 비교는 instrumentation 존재와 collection 활성화를 구분하는 데 유용하다.

## 분석 질문

- 긴 tail은 callback 계산 때문인가, dispatch 전 대기 때문인가?
- 어떤 publisher/subscription chain이 deadline miss와 연결되는가?
- Executor thread가 runnable인데 scheduling되지 않았는가?
- Logging/allocator/page fault/lock contention 직전에 spike가 생기는가?
- Trace buffer가 event를 잃었다면 latency 결론을 내릴 수 있는가?

## 참고

- [ros2/ros2_tracing 공식 저장소](https://github.com/ros2/ros2_tracing)
- [ROS 2 Jazzy tracetools_trace API](https://docs.ros.org/en/jazzy/p/tracetools_trace/tracetools_trace.trace.html)
