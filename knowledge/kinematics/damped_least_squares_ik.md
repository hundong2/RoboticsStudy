# Damped Least Squares Differential IK

## 문제

미분 기구학 `x_dot = J(q) q_dot`에서 작은 singular value는 pseudoinverse gain을 크게 만들어 joint velocity를 폭주시킨다. DLS는 Cartesian 오차와 joint speed를 함께 최소화한다.

\[
q_dot^*=\arg\min_{q_dot}\left(\|Jq_dot-x_dot_d\|^2+\lambda^2\|q_dot\|^2\right)
\]

\[
q_dot=J^T(JJ^T+\lambda^2I)^{-1}x_dot_d
\]

SVD의 각 방향 gain은 `1/sigma` 대신 `sigma/(sigma²+lambda²)`가 된다. 작은 singular value 방향을 억제하는 대신 말단 tracking error를 허용한다.

## 2R 평면 팔

\[
x=l_1\cos q_1+l_2\cos(q_1+q_2),\quad
y=l_1\sin q_1+l_2\sin(q_1+q_2)
\]

\[
J=\begin{bmatrix}
-l_1\sin q_1-l_2\sin(q_1+q_2) & -l_2\sin(q_1+q_2)\\
l_1\cos q_1+l_2\cos(q_1+q_2) & +l_2\cos(q_1+q_2)
\end{bmatrix}
\]

`det(J)=l1*l2*sin(q2)`이므로 `q2≈0` 또는 `q2≈pi`가 특이점이다. 교육용 adaptive damping은 `|det(J)|`가 threshold 아래일 때 lambda를 부드럽게 키울 수 있다. 고차원 로봇에서는 최소 singular value나 condition number가 더 직접적인 지표다.

## 제품화 체크리스트

- Cartesian error feedback에 gain과 velocity limit를 모두 둔다.
- 적분 step 후 joint position limit도 적용한다.
- unreachable target을 입력 경계에서 거부하거나 projection한다.
- singularity metric, lambda, Cartesian error, saturation count를 기록한다.
- joint-limit/collision/null-space objective가 필요하면 constrained QP를 고려한다.
- trajectory별 tracking error와 최대 joint speed를 fixed/adaptive damping 조건으로 비교한다.

## 근간 논문

- Nakamura and Hanafusa, [Inverse Kinematic Solutions With Singularity Robustness for Robot Manipulator Control](https://doi.org/10.1115/1.3143764), 1986.
