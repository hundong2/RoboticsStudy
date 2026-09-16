# 평면 IMU 사전적분 직관

짧은 구간의 body 가속도 `a_b`와 yaw 속도 `ω`로 `Δθ=ωΔt`, `a_o=R(θ+Δθ/2)a_b`, `Δv=a_oΔt`, `Δp=vΔt+0.5a_oΔt²`를 누적한다. 처음 자세/속도/위치를 영으로 두면 출력은 원점 기준 상대 변화다. 일정한 `a=0.5 m/s²`, `ω=0.3 rad/s`에 대한 평면 해석해는 `x=a/ω²(1-cos ωt)`, `y=a/ω²(ωt-sin ωt)`이고 감사 노드가 이를 독립 계산한다.

실제 IMU에는 중력, 자이로·가속도 바이어스, 축 정렬 및 시간 오차, 가속도 잡음이 있다. 이 실습의 `sensor_msgs/Imu.linear_acceleration`은 중력 제거된 이상적인 수평 가속도로 정의한다. 완전한 3D on-manifold preintegration은 `SO(3)` 회전, bias 보정 Jacobian, 공분산 전파와 키프레임 factor를 추가한다. 수학적 출처는 [Forster et al., T-RO 2017](https://arxiv.org/abs/1512.02363); 오늘 구현은 그 축약 예제이지 논문 알고리즘의 재현이 아니다.
