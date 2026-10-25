# Rectified Stereo 깊이와 불확실성

## 메시지/좌표 계약

ROS optical frame은 `+x right, +y down, +z forward`다. rectified 좌/우 영상은 같은 row에 대응점이 있어야 하며 disparity는 보통 `d=u_left-u_right`로 둔다. `CameraInfo::K`는 intrinsic, 오른쪽 `CameraInfo::P[3]`는 rectified x translation을 포함한다.

\[
B=-P_R(0,3)/P_R(0,0)
\]

baseline 단위가 m면 아래 깊이도 m다.

## Triangulation

\[
Z=\frac{f_xB}{d},\qquad
X=\frac{(u-c_x)Z}{f_x},\qquad
Y=\frac{(v-c_y)Z}{f_y}
\]

`d<=0`, NaN/Inf, 지나치게 작은 disparity, camera 유효범위 밖 depth는 model fitting 전에 거른다.

## Disparity noise의 depth 전파

\[
\frac{\partial Z}{\partial d}=-\frac{f_xB}{d^2},\qquad
\sigma_Z\approx\frac{f_xB}{d^2}\sigma_d
\]

먼 물체는 disparity가 작아져 같은 subpixel error에도 depth sigma가 빠르게 커진다. 따라서 stereo point를 동일 meter covariance로 취급하면 안 된다.

## 실무 점검표

- 좌/우 acquisition stamp와 exposure timing이 맞는가?
- image가 실제로 rectified되었고 K/P가 그 출력 해상도와 일치하는가?
- `P_R[3]`의 부호와 baseline 단위가 올바른가?
- disparity sigma가 matcher/texture/depth에 따라 달라지는가?
- 먼 점의 큰 sigma에 상한 또는 별도 max-range gate가 있는가?
- rolling shutter, 온도, 진동에 따른 extrinsic drift를 감시하는가?

대표 실습: `daily_robotics/2026-10-26`.
