# 비전 모델 학습 기초

모델 학습은 사진을 반복해서 보여주고 정답과의 차이를 줄이는 과정입니다. 한 번의 전체 데이터 학습을 `epoch`, 한 번에 처리하는 작은 데이터 묶음을 `batch`, 모델을 얼마나 크게 수정할지를 `learning rate`라고 합니다.

## 먼저 정할 것

문제 유형(분류/탐지/분할/추적), 실제 행동으로 이어질 metric, 오탐과 미탐의 비용, 목표 FPS·전력·메모리를 먼저 적습니다. 장치 제약을 마지막에 고려하면 정확하지만 배포할 수 없는 모델이 나옵니다.

transfer learning은 대규모 사전학습 weight에서 시작해 작은 learning rate로 task head와 backbone을 조정합니다. 초반에는 backbone freeze로 baseline을 만들고, 데이터가 충분하면 점진적으로 unfreeze합니다. train loss가 낮다는 이유만으로 채택하지 말고 촬영 장소/날짜/카메라가 겹치지 않는 holdout에서 비교합니다.

- `weight`: 모델이 학습한 숫자 값
- `backbone`: 이미지의 기본 특징을 찾는 앞부분
- `head`: 특징을 이용해 클래스와 위치를 예측하는 뒷부분
- `freeze`: 일부 weight를 바꾸지 않고 학습하는 것
- `baseline`: 이후 실험과 비교할 가장 단순한 첫 결과
- `loss`: 예측이 정답과 얼마나 다른지 나타내는 학습용 숫자
- `validation/holdout`: 학습에 사용하지 않고 성능 확인에만 사용하는 데이터

과적합 신호는 train metric 개선과 validation metric 악화, 특정 배경에 대한 의존, confidence 과대입니다. 더 다양한 실제 데이터, weight decay, 적절한 augmentation, early stopping으로 대응하되 validation leakage를 먼저 의심합니다.
