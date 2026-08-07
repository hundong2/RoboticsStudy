# 딥러닝 기반 로봇 정책 학습 커리큘럼

이 트랙은 ROS 2 기본기를 익힌 학습자가 딥러닝 기반 로봇 정책 학습 연구자 또는 엔지니어로 성장하기 위한 대학원식 실습 커리큘럼입니다. 기초 지식이 없는 학습자도 Python, PyTorch, 로봇 데이터, 정책 학습, 생성 모델, 시뮬레이터 closed-loop 평가, sim-to-real, 온디바이스 최적화, 논문 활동까지 단계적으로 도달하도록 설계했습니다.

기준일: 2026-07-13

## 학습 목표

이 과정을 끝낸 학습자는 다음 역량을 증명해야 합니다.

- Behavior Cloning, Imitation Learning, Reinforcement Learning, Offline RL 중 최소 2개를 직접 구현하고 비교한다.
- Diffusion, Flow Matching, VAE 중 최소 2개 생성 모델을 PyTorch로 구현하고 로봇 action generation에 연결한다.
- Python/PyTorch 기반 데이터셋, 학습 루프, 평가 루프, checkpoint, experiment tracking 파이프라인을 만든다.
- Visual Navigation, Long-Horizon/Mapless Navigation, Topological Map/Memory 기반 Navigation 논문을 읽고 재현 계획을 세운다.
- VLA, World Model, 생성형 시뮬레이션, 3D Gaussian Splatting의 역할과 한계를 설명한다.
- CARLA, Isaac Sim, Gazebo/Webots 중 하나 이상으로 closed-loop 평가를 설계한다.
- ONNX Runtime 또는 TensorRT로 모델을 최적화하고 latency, throughput, memory를 측정한다.
- CVPR, ICCV, ECCV, NeurIPS, CoRL, ICRA, IROS 스타일의 연구 질문, 실험표, ablation, failure analysis를 작성한다.

## 폴더 구조

```text
robot_learning/
  README.md
  00_foundations/
  01_pytorch_pipeline/
  02_policy_learning_il_bc/
  03_rl_offline_rl/
  04_generative_models/
  05_visual_navigation_memory/
  06_vla_world_models_generative_sim/
  07_simulators_closed_loop/
  08_sim_to_real_deployment/
  09_on_device_inference/
  10_research_publication/
  11_capstone/
  daily_practice/
  references/
```

## 전체 로드맵

| 단계 | 기간 | 카테고리 | 핵심 질문 | 산출물 |
|---|---:|---|---|---|
| 0 | 2주 | [기초 수학/프로그래밍/로봇](00_foundations/README.md) | tensor, 확률, 최적화, 좌표계를 설명할 수 있는가? | Python/PyTorch 노트북, 좌표계 요약 |
| 1 | 2주 | [PyTorch 학습 파이프라인](01_pytorch_pipeline/README.md) | 데이터셋부터 checkpoint까지 재현 가능한가? | 학습 템플릿, config, metric log |
| 2 | 3주 | [Imitation Learning/Behavior Cloning](02_policy_learning_il_bc/README.md) | demonstration으로 policy를 학습할 수 있는가? | BC policy, train/eval report |
| 3 | 3주 | [RL/Offline RL](03_rl_offline_rl/README.md) | reward와 offline dataset의 한계를 이해하는가? | PPO/SAC 개념 구현, offline RL 비교표 |
| 4 | 4주 | [생성 모델](04_generative_models/README.md) | VAE, diffusion, flow matching이 policy에 왜 쓰이는가? | VAE/action diffusion toy model |
| 5 | 4주 | [Visual Navigation/Memory](05_visual_navigation_memory/README.md) | 지도 없이 이미지와 memory로 이동할 수 있는가? | visual nav 데이터셋 설계, topological memory prototype |
| 6 | 4주 | [VLA/World Model/생성형 시뮬레이션](06_vla_world_models_generative_sim/README.md) | 언어, 시각, action, 예측 모델을 어떻게 연결하는가? | VLA 인터페이스 설계, world model rollout 분석 |
| 7 | 3주 | [시뮬레이터 closed-loop 평가](07_simulators_closed_loop/README.md) | open-loop metric과 closed-loop success가 왜 다른가? | CARLA/Isaac/Gazebo 평가 계획 |
| 8 | 3주 | [Sim-to-Real/배포](08_sim_to_real_deployment/README.md) | 현실 로봇에서 gap을 어떻게 줄이는가? | domain randomization, safety checklist |
| 9 | 2주 | [온디바이스 추론 최적화](09_on_device_inference/README.md) | PyTorch 모델을 로봇 보드에서 빠르게 돌릴 수 있는가? | ONNX export, latency benchmark |
| 10 | 2주 | [논문/학술 활동](10_research_publication/README.md) | 연구 질문을 실험으로 증명할 수 있는가? | paper review, ablation table, poster outline |
| 11 | 4주 | [캡스톤](11_capstone/README.md) | navigation policy를 end-to-end로 검증했는가? | 코드, 리포트, 데모, 실패 분석 |

## 24주 강의 계획

| 주차 | 강의 | 실습 | 평가 |
|---:|---|---|---|
| 1 | Python, NumPy, tensor, 자동미분 | tensor 연산 30문제 | 퀴즈 |
| 2 | 확률, 최적화, 좌표계, ROS data flow | pose/action/state 정의 | 미니 리포트 |
| 3 | PyTorch Dataset/DataLoader/model/loss | train loop 작성 | 코드 리뷰 |
| 4 | config, checkpoint, seed, logging | 재현 가능한 실험 실행 | 실험 로그 |
| 5 | Behavior Cloning 기본 | state-action MLP BC | train/eval curve |
| 6 | 이미지 조건 정책 | CNN encoder + action head | 실패 사례 5개 |
| 7 | sequence policy, ACT/VQ-BeT 개념 | action chunking 실험 | ablation |
| 8 | RL 기본: MDP, value, policy gradient | CartPole/Pendulum baseline | metric table |
| 9 | SAC/PPO 개념, reward 설계 | reward shaping 비교 | 분석 리포트 |
| 10 | Offline RL: distribution shift, CQL/IQL | offline dataset split | OOD 분석 |
| 11 | VAE와 latent action | action VAE toy 구현 | latent visualization |
| 12 | Diffusion model 기본 | 2D action diffusion | sampling report |
| 13 | Flow Matching 기본 | velocity field toy 구현 | FM vs diffusion 비교 |
| 14 | Diffusion Policy/NoMaD 읽기 | receding horizon policy 설계 | 논문 발표 |
| 15 | Visual Navigation, ViNT/GNM/NoMaD | image-goal dataset schema | data card |
| 16 | Long-horizon, topological memory | graph memory prototype | route replay |
| 17 | VLA 모델과 robot foundation model | language-action interface | API spec |
| 18 | World Model과 model-based control | latent rollout 분석 | rollout error |
| 19 | 3DGS/생성형 시나리오 | scenario generation plan | synthetic data plan |
| 20 | Gazebo/Isaac/CARLA closed-loop | success/collision/SPL metric | eval protocol |
| 21 | Sim-to-real: domain randomization, calibration | gap checklist | risk register |
| 22 | ONNX Runtime/TensorRT 최적화 | export/benchmark | latency table |
| 23 | 연구 논문 작성법 | related work matrix | 2-page proposal |
| 24 | 캡스톤 발표 | demo + failure analysis | 최종 평가 |

## 매일 학습 루틴

자세한 일일 루틴은 [daily_practice](daily_practice/README.md)에 있습니다.

| 시간 | 활동 | 산출물 |
|---:|---|---|
| 30분 | 전날 코드/노트 복습 | 막힌 점 3개 |
| 60분 | 강의 개념 구현 | 작은 Python/PyTorch 파일 |
| 60분 | 실험 1개 실행 | metric log |
| 30분 | 논문 또는 공식 문서 읽기 | 5문장 요약 |
| 30분 | 실패 분석 | 원인 가설, 다음 실험 |

## 카테고리별 평가 루브릭

| 레벨 | 설명 | 증거 |
|---|---|---|
| 입문 | 코드를 따라 실행한다 | 명령어 로그, 그래프 캡처 |
| 초급 | 핵심 개념을 자기 말로 설명한다 | 요약 노트, 퀴즈 |
| 중급 | 작은 모델과 학습 루프를 직접 만든다 | 재현 가능한 코드 |
| 고급 | 실패를 분석하고 실험을 설계한다 | ablation, failure report |
| 전문가 | 논문 수준의 질문과 검증을 구성한다 | proposal, closed-loop demo, 배포 리포트 |

## 권장 선수 지식

- Python 함수, 클래스, 파일 입출력
- NumPy 배열과 PyTorch tensor
- 미분, 확률분포, 기댓값, KL divergence의 직관
- ROS 2 topic, message, TF, rosbag2
- 카메라 이미지, pose, odometry, action command의 의미

## 필수 프로젝트

1. Behavior Cloning으로 2D navigation policy 학습
2. Offline dataset에서 BC와 offline RL baseline 비교
3. VAE 또는 diffusion으로 multi-modal action 생성
4. Visual Navigation memory/topological map prototype
5. Gazebo, Isaac Sim, CARLA 중 하나에서 closed-loop 평가 계획 수립
6. ONNX Runtime 또는 TensorRT 추론 최적화 벤치마크
7. 캡스톤: visual navigation policy 연구 리포트와 데모

## 참고 자료

상세 링크는 [references](references/README.md)에 모았습니다.
