# 04. 생성 모델: VAE, Diffusion, Flow Matching

목표는 생성 모델을 이미지 생성 기술로만 보지 않고, 로봇 action distribution과 multi-modal behavior를 표현하는 도구로 이해하는 것입니다.

## 강의 목표

- VAE의 encoder, latent, decoder, KL term을 구현한다.
- Diffusion의 forward noising과 reverse denoising을 구현한다.
- Flow Matching의 velocity field regression 개념을 이해한다.
- 생성 모델을 action sequence policy에 연결한다.

## 모델별 핵심

| 모델 | 직관 | 로봇 정책 활용 |
|---|---|---|
| VAE | 데이터를 latent 공간으로 압축하고 복원 | latent skill, action embedding |
| Diffusion | noise에서 점진적으로 action을 복원 | multi-modal action generation |
| Flow Matching | noise에서 data로 가는 흐름의 velocity 학습 | 빠른 sampling, continuous action path |
| 3D Gaussian Splatting | 장면을 3D Gaussian으로 표현하고 빠르게 렌더링 | synthetic view, scene reconstruction |

## 데일리 실습

| 일차 | 실습 | 산출물 |
|---:|---|---|
| 1 | Gaussian likelihood와 MSE 관계 | 수식 노트 |
| 2 | VAE encoder/decoder 구현 | latent scatter plot |
| 3 | reparameterization trick 구현 | gradient 확인 |
| 4 | action VAE | action reconstruction |
| 5 | DDPM forward process | noisy action plot |
| 6 | denoising network 구현 | sampling gif 또는 plot |
| 7 | conditional diffusion | goal-conditioned action sample |
| 8 | flow matching toy | vector field plot |
| 9 | diffusion vs flow matching 비교 | sampling step/quality 표 |
| 10 | 로봇 policy 연결 설계 | architecture diagram |

## 실습 과제

1. 2D 경로 데이터에서 여러 가능한 action mode를 만든다.
2. MSE BC, VAE policy, diffusion policy를 비교한다.
3. 같은 observation에서 가능한 action sample 10개를 시각화한다.
4. sampling step 수와 latency의 trade-off를 분석한다.

## 통과 기준

- multi-modal action에서 평균 action이 위험할 수 있음을 설명한다.
- VAE의 KL collapse와 diffusion의 sampling cost를 설명한다.
- action generation 모델을 closed-loop controller로 쓸 때 receding horizon이 필요한 이유를 설명한다.
