# 비전 모델 학습 기초

## 먼저 정할 것

문제 유형(분류/탐지/분할/추적), 실제 행동으로 이어질 metric, 오탐과 미탐의 비용, 목표 FPS·전력·메모리를 먼저 적습니다. 장치 제약을 마지막에 고려하면 정확하지만 배포할 수 없는 모델이 나옵니다.

transfer learning은 대규모 사전학습 weight에서 시작해 작은 learning rate로 task head와 backbone을 조정합니다. 초반에는 backbone freeze로 baseline을 만들고, 데이터가 충분하면 점진적으로 unfreeze합니다. train loss가 낮다는 이유만으로 채택하지 말고 촬영 장소/날짜/카메라가 겹치지 않는 holdout에서 비교합니다.

과적합 신호는 train metric 개선과 validation metric 악화, 특정 배경에 대한 의존, confidence 과대입니다. 더 다양한 실제 데이터, weight decay, 적절한 augmentation, early stopping으로 대응하되 validation leakage를 먼저 의심합니다.

