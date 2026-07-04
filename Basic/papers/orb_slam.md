# ORB-SLAM: A Versatile and Accurate Monocular SLAM System

- 원문: https://ieeexplore.ieee.org/document/7219438/
- 공개 PDF 예시: https://courses.cs.washington.edu/courses/csep576/21au/resources/ORB-SLAM_A_Versatile_and_Accurate_Monocular_SLAM_System.pdf
- 저자: Raul Mur-Artal, J. M. M. Montiel, Juan D. Tardos
- 발표: IEEE Transactions on Robotics, 2015

## 왜 읽는가

Visual SLAM 시스템이 실제로 어떤 모듈로 구성되는지 보기 좋은 대표 논문이다. 처음 읽을 때는 ORB descriptor 자체보다 `tracking`, `local mapping`, `loop closing`의 분리를 보자.

## 핵심 번역식 요약

- 단안 카메라만으로 실시간 SLAM을 수행한다.
- ORB feature를 사용해 빠르게 추적하고 매칭한다.
- tracking thread는 현재 프레임의 카메라 pose를 추정한다.
- local mapping thread는 keyframe과 map point를 관리한다.
- loop closing thread는 이전 장소 재방문을 찾아 누적 drift를 줄인다.
- relocalization은 tracking 실패 후 다시 위치를 회복하는 기능이다.

## 읽는 순서

1. Abstract와 Introduction에서 시스템 목표 확인
2. System Overview figure 확인
3. Tracking 섹션
4. Local Mapping 섹션
5. Loop Closing 섹션
6. Experiments는 어떤 데이터셋으로 평가했는지만 먼저 확인

## 확인 질문

- 왜 모든 프레임을 keyframe으로 쓰지 않는가?
- loop closure가 없으면 어떤 문제가 생기는가?
- 단안 SLAM은 scale ambiguity 문제를 왜 가지는가?
- relocalization은 실무에서 왜 중요한가?
