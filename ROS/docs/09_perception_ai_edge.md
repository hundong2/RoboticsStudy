# 09. Perception, AI, Edge

로봇 perception은 카메라, LiDAR, IMU 같은 센서를 해석해 로봇이 환경을 이해하도록 만드는 영역입니다. 2026년 기준으로는 전통적인 geometry 기반 perception과 딥러닝/VLM 기반 perception이 함께 쓰입니다.

## ROS에서 자주 쓰는 센서 메시지

| 메시지 | 용도 |
|---|---|
| `sensor_msgs/msg/Image` | 카메라 이미지 |
| `sensor_msgs/msg/CameraInfo` | 카메라 내부 파라미터 |
| `sensor_msgs/msg/LaserScan` | 2D LiDAR |
| `sensor_msgs/msg/PointCloud2` | 3D point cloud |
| `sensor_msgs/msg/Imu` | 관성 센서 |

## AI 노드 설계 원칙

- 입력 topic과 출력 topic을 명확히 나눕니다.
- 추론 latency를 로그로 남깁니다.
- GPU 사용 시 copy 비용을 측정합니다.
- model version과 preprocessing 설정을 기록합니다.
- rosbag2로 같은 입력에 같은 결과가 나오는지 확인합니다.

## Edge 배포 체크리스트

- FPS와 latency 목표가 있는가?
- CPU/GPU/NPU 사용률을 측정했는가?
- 발열과 전력 제한을 고려했는가?
- network 없이 동작해야 하는 기능을 분리했는가?
- 안전 관련 판단을 AI 단독 결정에 맡기지 않았는가?

## 통과 기준

- perception 노드가 `/image_raw`를 받아 `/detections`를 내보내는 흐름을 설계한다.
- AI 추론 결과가 Nav2 또는 MoveIt2와 연결될 때 필요한 좌표 변환을 설명한다.
- 로봇에서 "정확도"뿐 아니라 latency, 안정성, 관측성을 함께 봐야 하는 이유를 설명한다.
