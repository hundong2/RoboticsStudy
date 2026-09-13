# Elastic Band 경로 변형

## 핵심 모델

전역 planner가 만든 collision-free path를 변형 가능한 band로 보고, 내부 수축력과 장애물 외력의 평형을 반복 계산한다.

```text
F_internal = k_c (p_(i-1) - 2 p_i + p_(i+1))
F_external = obstacle-distance gradient
```

내부 힘은 이산 곡률을 낮추고, 외부 힘은 clearance를 만든다. 시작·목표 knot는 고정한다. 계산량 상한이 필요하면 knot 수, obstacle 수, 반복 횟수를 모두 제한한다.

## 실패 판정

- footprint clearance가 안전 기준 아래로 내려감
- 반복 후에도 cost가 감소하지 않음
- knot 사이 swept segment가 장애물과 충돌함
- band가 지나던 통로의 위상이 끊김

마지막 경우는 local deformation으로 해결하지 말고 global replanning을 요청한다.

## 현대 확장

시간 변수를 함께 최적화하면 속도·가속도 제약을 넣을 수 있다. 실제 제품에서는 비홀로노믹 제약, 동적 장애물의 미래 위치, footprint 연속 충돌, solver deadline을 포함하고 독립 safety monitor를 둔다.

근간 논문: [Quinlan & Khatib, ICRA 1993](https://doi.org/10.1109/ROBOT.1993.291936)
