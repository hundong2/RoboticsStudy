# 01. PyTorch 기반 학습 파이프라인

목표는 모델 하나를 만드는 수준을 넘어, 재현 가능한 학습 파이프라인을 직접 구성하는 것입니다.

## 강의 목표

- `Dataset`, `DataLoader`, `nn.Module`, optimizer, scheduler, loss를 구현한다.
- config 파일로 실험 조건을 관리한다.
- seed, train/val split, checkpoint, metric log를 남긴다.
- 학습 실패를 loss curve, gradient norm, sample visualization으로 진단한다.

## 표준 프로젝트 구조

```text
policy_project/
  configs/
    bc_mlp.yaml
  data/
    README.md
  src/
    datasets.py
    models.py
    train.py
    evaluate.py
    export_onnx.py
  outputs/
    run_001/
      config.yaml
      metrics.csv
      checkpoints/
      figures/
```

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | `Dataset`으로 trajectory 파일 읽기 | batch shape 출력 |
| 2 | MLP policy 작성 | forward pass test |
| 3 | train loop 작성 | loss curve |
| 4 | validation split 추가 | val metric |
| 5 | checkpoint 저장/재시작 | resume log |
| 6 | YAML config 적용 | config snapshot |
| 7 | seed 고정과 재현성 테스트 | 3회 실행 결과 |
| 8 | gradient norm/logging | exploding/vanishing 분석 |
| 9 | tensorboard 또는 csv logger | experiment folder |
| 10 | inference script 작성 | single sample prediction |

## 실습 과제

1. 2D waypoint dataset을 만든다.
2. MLP policy를 학습한다.
3. 학습률 3개를 비교한다.
4. 가장 좋은 checkpoint를 저장하고 inference script로 불러온다.
5. 실험 조건과 결과를 `metrics.csv`와 `README.md`에 남긴다.

## 통과 기준

- 다른 사람이 같은 config로 같은 실험을 재현할 수 있다.
- overfit, underfit, data leakage를 구분한다.
- 모델 파일만이 아니라 preprocessing과 action scale도 함께 저장해야 함을 설명한다.
