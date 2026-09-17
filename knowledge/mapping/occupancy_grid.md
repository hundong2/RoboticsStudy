# 점유 격자와 로그 오즈

각 셀은 장애물이 있을 확률 `p`를 갖는다. ROS `nav_msgs/msg/OccupancyGrid`는 `info.resolution`(m/셀), `info.origin`, `info.width/height`, `data`를 사용한다. 배열은 row-major여서 `(col,row)`의 인덱스가 `row×width+col`이다. 이 실습은 확률을 0~100 정수로 보내고 **관측하지 않은 셀**을 `−1`로 표시한다. ROS 메시지 정의는 셀 값 자체를 응용별로 해석하도록 하므로 소비자와 이 계약을 공유해야 한다.

로그 오즈 `l=log(p/(1−p))`는 증거를 더하기 쉽다. 역센서 모델이 자유 셀에 음수, 실제 반사 끝점에 양수 증분을 주고, `p=1/(1+exp(−l))`로 다시 확률화한다. 독립 관측 가정과 센서 모델의 보정이 있어야 수치에 통계적 의미가 있다. 같은 스캔을 반복해 확신도가 과도하게 커지는 문제를 막으려면 증분·포화값·시간 감쇠를 신중히 정한다.

광선을 일정 거리 간격으로 따라가면 같은 셀을 여러 번 만날 수 있으므로 광선당 한 번만 갱신한다. 끝점 직전은 free, 실제 반사 끝점은 occupied로 처리한다. 반사가 없는 광선은 지도/센서 계약에 따라 free만 표시하거나 관측을 건너뛴다. 포즈가 틀리면 모든 셀이 함께 어긋나므로 지도 품질은 위치 추정·시간 동기화·TF에 의존한다.

참고: [Jazzy OccupancyGrid 원본 메시지](https://github.com/ros2/common_interfaces/blob/jazzy/nav_msgs/msg/OccupancyGrid.msg), [Moravec–Elfes 원문](https://www.ri.cmu.edu/pub_files/pub3/moravec_hans_1985_2/moravec_hans_1985_2.pdf).
