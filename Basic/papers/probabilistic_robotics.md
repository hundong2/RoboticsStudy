# Probabilistic Robotics

- 원문/도서 정보: https://mitpress.mit.edu/9780262201629/probabilistic-robotics/
- 저자: Sebastian Thrun, Wolfram Burgard, Dieter Fox
- 성격: 로봇 확률 추정 분야의 표준 교재

## 왜 읽는가

로봇은 센서가 부정확하고, 바퀴가 미끄러지고, 환경이 변한다. 그래서 로봇공학은 많은 문제를 `정답을 직접 계산하는 문제`가 아니라 `불확실한 상태를 추정하는 문제`로 다룬다.

## 핵심 번역식 요약

- 로봇은 세계를 완전히 알 수 없으므로 belief를 유지한다.
- motion model은 로봇이 움직였을 때 상태가 어떻게 바뀔지 예측한다.
- sensor model은 관측값이 현재 상태와 얼마나 일치하는지 계산한다.
- Bayes filter는 예측과 관측을 반복해 belief를 갱신하는 공통 골격이다.
- SLAM은 로봇 pose와 지도를 동시에 추정하는 확률 문제로 볼 수 있다.

## 읽는 순서

1. Bayes filter의 예측-갱신 구조
2. Kalman Filter와 Extended Kalman Filter
3. Particle Filter
4. Occupancy Grid Mapping
5. SLAM 장

## 확인 질문

- localization과 mapping은 각각 무엇을 추정하는가?
- covariance가 커진다는 것은 무슨 뜻인가?
- 센서 관측이 들어오면 왜 불확실성이 줄어드는가?
- particle filter는 왜 여러 개의 샘플을 쓰는가?
