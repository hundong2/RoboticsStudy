# 4. GPIO와 PWM

## 4.1 GPIO 기본

GPIO 입력은 switch, interrupt, fault 신호를 읽고 출력은 enable, reset 같은 논리 제어에 쓴다. JetPack 6 이후 구형 `/sys/class/gpio` 방식은 deprecated이므로 `/dev/gpiochipN` character-device API와 `gpiodetect`, `gpioinfo`를 기준으로 한다.

물리 핀에서 Linux line offset으로 가는 과정:

```text
J12 물리 핀 → Carrier Spec의 SoC signal → 현재 pinmux/DT → gpiochip label + line offset
```

`gpio_v2_toggle`은 사용자가 확인한 gpiochip과 offset만 받는다. 임의의 offset을 넣으면 다른 기능을 건드릴 수 있다.

```bash
gpiodetect
gpioinfo
./build/bin/gpio_v2_toggle /dev/gpiochip0 <확인한_offset> 10 200
```

LED를 연결할 때는 GPIO로 직접 큰 전류를 흘리지 않는다. 로직 입력이나 buffer/MOSFET gate를 대상으로 먼저 실습한다. 출력 상태가 boot 중 glitch를 만들면 외부 pull과 driver enable로 안전 상태를 강제한다.

## 4.2 edge 입력의 실무 원칙

- push button은 bounce가 있으므로 시간 기반 debounce가 필요하다.
- interrupt line은 active level과 pull을 확인한다.
- 이벤트 timestamp와 monotonic clock을 사용한다.
- 안전 입력은 단일 userspace GPIO만 믿지 말고 하드웨어 interlock·watchdog를 함께 둔다.

## 4.3 PWM 기본

PWM은 일정 period 안에서 High 시간(duty cycle)을 바꾼다.

```text
frequency = 1 / period
duty(%)   = high_time / period × 100
```

J12의 PWM 후보 핀은 15, 32, 33이다. 실제 PWM controller와 channel은 pinmux/DT에 따라 달라지므로 `/sys/class/pwm/pwmchip*`와 device tree를 확인한다.

```bash
ls -l /sys/class/pwm
# 예: pwmchip0 channel 0, 1 kHz, 25%, 3초
sudo ./build/bin/pwm_sysfs /sys/class/pwm/pwmchip0 0 1000 25 3000
```

예제는 실행 시간이 끝나면 PWM을 disable하고, 자신이 export한 channel만 unexport한다. 서보·팬·모터는 각각 요구 주파수와 전기 driver가 다르다. PWM 핀에서 부하 전원을 직접 공급하지 않는다.

오실로스코프나 로직 애널라이저로 실제 주파수·duty와 boot/reboot 시 상태를 측정해야 실습이 완료된다.

