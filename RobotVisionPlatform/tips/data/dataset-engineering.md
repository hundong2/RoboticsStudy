# 데이터셋 엔지니어링

- 영상의 인접 프레임을 무작위 split하지 않습니다. clip/site/device 단위 group split을 사용합니다.
- 클래스마다 정상, 가림, 역광, 야간, motion blur, 작은 물체, 빈 장면 bucket을 관리합니다.
- 라벨 정의에는 포함/제외 예시와 최소 크기, 가림 처리 규칙을 둡니다.
- 중복 프레임을 줄이고 hard negative를 의도적으로 포함합니다.
- 증강은 실제 카메라 현상을 모사할 때만 사용하고 validation에는 적용하지 않습니다.
- dataset version에 원본 checksum, 라벨 schema, split seed, license/consent를 기록합니다.

초기에는 전체 영상을 올리지 말고 edge에서 낮은 confidence, 규칙 위반, 주기적 unbiased sample을 선정하는 것이 비용과 개인정보 측면에서 유리합니다.

