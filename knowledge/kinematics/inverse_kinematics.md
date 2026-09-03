# 역기구학(Inverse Kinematics) 핵심 노트

## FK와 IK

- 순기구학(FK): 관절 상태 `q`에서 end-effector pose `T=f(q)` 계산. 보통 해가 유일하고 계산이 직접적이다.
- 역기구학(IK): 목표 pose `T*`를 만족하는 `q` 탐색. 해가 0개, 1개, 여러 개 또는 무한히 많을 수 있다.

## 평면 2R 닫힌형 해

링크 길이 `L1`, `L2`, 목표 `(x,y)`에 대해:

```text
c2 = (x²+y²-L1²-L2²)/(2 L1 L2)
s2 = ±sqrt(1-c2²)
q2 = atan2(s2,c2)
q1 = atan2(y,x) - atan2(L2 s2, L1+L2 c2)
```

도달 조건은 `|L1-L2| ≤ sqrt(x²+y²) ≤ L1+L2`다. `±` 두 분기는 elbow configuration을 나타낸다.

## 실무 solver 체크리스트

1. joint limit와 self/environment collision을 필터링한다.
2. 이전 관절 상태와 가까운 해를 골라 궤적 연속성을 지킨다.
3. 특이점 근처에서는 Jacobian condition number와 속도 증폭을 감시한다.
4. 모든 후보를 FK로 되계산해 position/orientation 잔차를 확인한다.
5. “작업 반경 안”과 “요구 자세까지 포함해 도달 가능”을 구분한다.

## 해석해와 수치해

해석 IK는 빠르고 여러 분기를 명시하기 좋지만 로봇 구조에 특화된다. Jacobian 기반 수치 IK는 일반적이지만 초기값, local minimum, singularity, 종료 조건에 민감하다. 제품에서는 구조별 해석 solver와 수치 fallback을 함께 두기도 한다.

## 참고

- [Okazaki et al., CGS-based 6-DOF IK (2025)](https://arxiv.org/abs/2509.00823)
