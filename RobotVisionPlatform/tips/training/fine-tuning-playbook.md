# Fine-tuning 실전 플레이북

1. pretrained checkpoint로 재현 가능한 baseline을 만든다.
2. seed, code commit, dataset version, hyperparameter, hardware를 기록한다.
3. 20~50개 샘플을 과적합시켜 데이터/라벨/코드 경로가 정상인지 확인한다.
4. input size, batch size, learning rate를 작은 sweep으로 찾는다.
5. augmentation은 하나씩 추가해 ablation한다.
6. class별 PR curve와 운영 threshold를 선택한다.
7. ONNX export 전후 output을 동일 입력으로 비교한다.
8. 실제 Jetson에서 warm-up 후 latency/전력/온도/메모리를 측정한다.

mixed precision은 성능에 유리하지만 작은 물체나 후처리에서 수치 오차를 확인합니다. INT8은 대표 calibration set이 필요하며 정확도 회귀를 별도 승인 조건으로 둡니다.

