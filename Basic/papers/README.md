# 필수 논문/문서 읽기 순서

이 폴더는 논문 원문 번역본이 아니라, 입문자가 논문을 읽기 전에 필요한 배경, 핵심 주장, 읽는 순서, 확인 질문을 정리한 학습 가이드입니다. 원문 링크를 함께 두었으니 저작권 문제가 없도록 원문은 링크로 읽고, 이 문서는 한국어 해설과 요약으로 사용합니다.

## 추천 순서

1. [probabilistic_robotics.md](probabilistic_robotics.md) - 로봇에서 확률을 왜 쓰는가
2. [orb_slam.md](orb_slam.md) - Visual SLAM 시스템이 어떻게 나뉘는가
3. [attention_is_all_you_need.md](attention_is_all_you_need.md) - Transformer의 핵심 구조
4. [clip.md](clip.md) - 이미지와 텍스트를 같은 공간에 놓는 방법
5. [palm_e.md](palm_e.md) - embodied multimodal model의 문제의식
6. [rt1.md](rt1.md) - 로봇 행동 데이터를 Transformer로 학습하는 관점
7. [rt2.md](rt2.md) - VLM을 행동 모델로 확장하는 VLA 관점
8. [isaac_sim_docs.md](isaac_sim_docs.md) - 실습 환경으로서 Isaac Sim

## 읽기 원칙

- 처음 읽을 때는 수식보다 `문제 정의`, `입력/출력`, `시스템 구성`, `평가 방식`을 먼저 본다.
- 논문 하나를 완벽하게 번역하려 하지 말고, "이 논문이 어떤 문제를 어떤 구조로 풀었는가"를 한 문장으로 말할 수 있으면 다음으로 넘어간다.
- 구현 논문은 architecture diagram과 pipeline figure를 먼저 본다.
