# 비영점 경계조건 5차 관절 궤적

## 목적

정지→정지 이동뿐 아니라 이미 움직이는 상태에서 다음 궤적으로 이어질 때 위치·속도·가속도가 끊기지 않게 한다. 한 구간의 여섯 경계조건을 정확히 만족하려면 계수 여섯 개인 5차 다항식이 가장 단순한 선택이다.

## 정규화 시간과 계수

`s=t/T ∈ [0,1]`에 대해:

\[
q(s)=c_0+c_1s+c_2s^2+c_3s^3+c_4s^4+c_5s^5
\]

\[
c_0=q_0,\quad c_1=T v_0,\quad c_2=\frac{T^2a_0}{2}
\]

\[
A=q_f-(c_0+c_1+c_2),\quad
B=T v_f-(c_1+2c_2),\quad
C=T^2a_f-2c_2
\]

\[
c_3=10A-4B+\frac{C}{2},\quad
c_4=-15A+7B-C,\quad
c_5=6A-3B+\frac{C}{2}
\]

미분할 때 정규화 시간의 chain rule을 잊지 않는다.

\[
v(t)=\frac{1}{T}\frac{dq}{ds},\qquad
a(t)=\frac{1}{T^2}\frac{d^2q}{ds^2},\qquad
j(t)=\frac{1}{T^3}\frac{d^3q}{ds^3}
\]

## 구현 체크리스트

- `T>0`과 모든 경계값의 finite 여부를 먼저 검사한다.
- 계수는 비 RT 계획 단계에서 한 번 계산하고 제어 tick에서는 샘플만 계산한다.
- Horner 형식으로 다항식과 미분식을 평가한다.
- 정확히 `s=0`, `s=1`에서 여섯 경계조건을 단위 테스트한다.
- 다축 동기화가 필요하면 공통 `T`를 쓰되, 각 축의 속도·가속도·jerk·토크 최대값을 별도로 검사한다.
- 구간 연결 시 앞 구간 종료 `(q,v,a)`와 다음 구간 시작 `(q,v,a)`가 같은지 확인한다.

## 중요한 한계

5차라는 사실만으로 제한 준수가 보장되지 않는다. `T`가 너무 짧으면 속도·가속도·jerk와 요구 토크가 커진다. joint limit, collision, actuator saturation, flexible mode도 자동으로 처리하지 않는다. 제한이 중요한 시스템에서는 time scaling, jerk-limited online generation, TOPP, 또는 dynamics-aware optimization을 결합한다.

## 연결 실습

- `daily_robotics/2026-09-19`: 단일 관절 정지→정지와 해석적 시간 하한
- `daily_robotics/2026-09-25`: 3축 비영점 경계조건, Action tolerance, 500 Hz 샘플링과 독립 감사
