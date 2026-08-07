# 11. 캡스톤: Visual Navigation Policy 연구 프로젝트

목표는 채용 요건에 대응되는 포트폴리오형 연구 프로젝트를 완성하는 것입니다.

## 프로젝트 주제

시각 기반 내비게이션 policy를 학습하고, 생성 모델 기반 action head 또는 memory/topological map을 결합해 closed-loop 평가를 수행합니다.

## 필수 요구사항

- PyTorch 기반 학습 파이프라인
- Behavior Cloning baseline
- 생성 모델 기반 policy head 또는 latent action model
- Visual Navigation 또는 mapless/topological memory 구성
- Gazebo, Webots, Isaac Sim, CARLA 중 하나의 closed-loop 평가 설계
- sim-to-real gap 분석
- ONNX Runtime 또는 TensorRT 최적화 계획
- 논문 형식 리포트

## 권장 아키텍처

```text
camera image/history
  -> vision encoder
  -> memory/topological retrieval
  -> policy backbone
  -> action head
       |-- MSE BC baseline
       |-- VAE latent action
       |-- diffusion/flow matching action sampler
  -> safety filter
  -> /cmd_vel or waypoint command
```

## 4주 실행 계획

| 주차 | 목표 | 산출물 |
|---:|---|---|
| 1 | 문제 정의와 데이터셋 | proposal, dataset schema |
| 2 | BC baseline과 평가 루프 | train/eval code, baseline table |
| 3 | 생성 모델 또는 memory 확장 | ablation, failure report |
| 4 | closed-loop 평가와 배포 계획 | final report, demo, poster |

## 최종 제출물

```text
capstone/
  README.md
  paper_draft.md
  configs/
  src/
  data_card.md
  model_card.md
  eval_protocol.md
  sim_to_real_report.md
  deployment_report.md
  failure_cases.md
```

## 평가 루브릭

| 영역 | 배점 | 기준 |
|---|---:|---|
| 문제 정의 | 15 | task, observation, action, metric이 명확함 |
| 데이터 | 15 | split, quality, coverage, bias 분석 |
| 모델 | 20 | BC baseline과 생성 모델/메모리 확장 비교 |
| 평가 | 20 | open-loop와 closed-loop metric 포함 |
| 배포 | 10 | latency, safety, sim-to-real 고려 |
| 연구성 | 20 | baseline, ablation, failure analysis, related work |

## 전문가 통과 기준

- policy가 언제 실패하는지 예측하고 재현한다.
- baseline보다 좋아진 이유를 ablation으로 설명한다.
- 모델 성능뿐 아니라 latency와 safety를 함께 보고한다.
- 연구 리포트가 학회 short paper 초안 수준의 구조를 갖는다.
