# Edge 평가

정확도 표에는 mAP50-95, class별 precision/recall, confidence threshold를 포함합니다. 시스템 표에는 capture-to-action latency p50/p95/p99, steady-state FPS, peak memory, power mode, 온도, throttling, 네트워크 사용량, drop rate를 포함합니다.

평가 시 cold start와 warm state를 분리하고 최소 30분 이상 열 평형 상태를 관찰합니다. 랜/Wi-Fi 정상, packet loss, 서버 단절, 재접속, 디스크 부족, 카메라 분리 시나리오를 반복합니다. 모델 후보는 정확도 하나가 아니라 이 제약의 Pareto frontier에서 선택합니다.

